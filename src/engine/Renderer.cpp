#include "engine/Renderer.h"
#include "model/Ids.h"

namespace harmony::engine
{
Renderer::Renderer (AudioEngine& e, RenderSettings s, std::function<void (juce::Result)> done)
    : juce::Thread ("Harmony render"), engine (e), settings (std::move (s)), onFinished (std::move (done))
{
}

Renderer::~Renderer()
{
    stopThread (30000);
}

juce::StringArray Renderer::formatNames()
{
    return { "WAV", "FLAC", "OGG Vorbis", "AIFF" };
}

juce::String Renderer::extensionFor (const juce::String& name)
{
    if (name == "FLAC")       return ".flac";
    if (name == "OGG Vorbis") return ".ogg";
    if (name == "AIFF")       return ".aiff";
    return ".wav";
}

std::unique_ptr<juce::AudioFormat> Renderer::createFormat (const juce::String& name)
{
    if (name == "FLAC")       return std::make_unique<juce::FlacAudioFormat>();
    if (name == "OGG Vorbis") return std::make_unique<juce::OggVorbisAudioFormat>();
    if (name == "AIFF")       return std::make_unique<juce::AiffAudioFormat>();
    return std::make_unique<juce::WavAudioFormat>();
}

juce::Array<int> Renderer::bitDepthsFor (const juce::String& name)
{
    if (name == "FLAC")       return { 16, 24 };
    if (name == "OGG Vorbis") return { 16 };
    if (name == "AIFF")       return { 16, 24 };
    return { 16, 24, 32 };
}

void Renderer::start()
{
    engine.beginOffline (settings.sampleRate, 1024, settings.startTick, settings.songMode);
    startThread (juce::Thread::Priority::high);
}

std::unique_ptr<juce::AudioFormatWriter> Renderer::openWriter (const juce::File& file, juce::AudioFormat& format,
                                                               bool floatingPoint, int bits) const
{
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
    if (! static_cast<juce::FileOutputStream*> (stream.get())->openedOk())
        return nullptr;

    const auto options = juce::AudioFormatWriterOptions{}
                             .withSampleRate (settings.sampleRate)
                             .withNumChannels (2)
                             .withBitsPerSample (bits)
                             .withQualityOptionIndex (format.getQualityOptions().isEmpty() ? 0 : juce::jlimit (0, format.getQualityOptions().size() - 1, settings.oggQuality))
                             .withSampleFormat (floatingPoint ? juce::AudioFormatWriterOptions::SampleFormat::floatingPoint
                                                              : juce::AudioFormatWriterOptions::SampleFormat::integral)
                             .withMetadata (juce::WavAudioFormat::bwavOriginator, "Yannosay Harmony");

    return format.createWriterFor (stream, options);
}

void Renderer::run()
{
    auto result = renderAll();
    progress = 1.0f;

    juce::MessageManager::callAsync ([this, result]
    {
        engine.endOffline();
        auto done = onFinished;
        if (done)
            done (result);
    });
}

juce::Result Renderer::renderAll()
{
    const double samplesPerTick = settings.sampleRate * 60.0 / (juce::jmax (10.0, settings.bpm) * ppq);
    const auto musicalSamples = (juce::int64) std::ceil ((double) juce::jmax ((juce::int64) 1, settings.endTick - settings.startTick) * samplesPerTick);
    const auto tailSamples = (juce::int64) (juce::jmax (0.0, settings.tailSeconds) * settings.sampleRate);
    const auto totalSamples = musicalSamples + tailSamples;

    settings.output.getParentDirectory().createDirectory();
    const auto tempFile = settings.output.getSiblingFile (settings.output.getFileNameWithoutExtension() + ".harmony-render.wav");

    juce::WavAudioFormat wav;
    auto mixWriter = openWriter (tempFile, wav, true, 32);
    if (mixWriter == nullptr)
        return juce::Result::fail ("Cannot write to " + settings.output.getParentDirectory().getFullPathName());

    struct Stem
    {
        int insert;
        std::unique_ptr<juce::AudioFormatWriter> writer;
    };

    std::vector<Stem> stems;
    auto stemFormat = createFormat (settings.format);
    for (auto& [index, name] : settings.stemInserts)
    {
        const auto safeName = juce::File::createLegalFileName (juce::String (index).paddedLeft ('0', 2) + " " + name);
        const auto stemFile = settings.output.getSiblingFile (settings.output.getFileNameWithoutExtension() + " - Stems")
                                  .getChildFile (safeName + extensionFor (settings.format));
        stemFile.getParentDirectory().createDirectory();

        auto writer = openWriter (stemFile, *stemFormat, settings.bitDepth == 32, settings.bitDepth);
        if (writer == nullptr)
            return juce::Result::fail ("Cannot write stem " + stemFile.getFullPathName());
        stems.push_back ({ index, std::move (writer) });
    }

    juce::AudioBuffer<float> block (2, 1024);
    float peak = 0.0f;
    juce::int64 done = 0;
    bool transportStopped = false;

    while (done < totalSamples)
    {
        if (threadShouldExit())
        {
            mixWriter.reset();
            tempFile.deleteFile();
            return juce::Result::fail ("Render cancelled.");
        }

        int n = (int) juce::jmin ((juce::int64) block.getNumSamples(), totalSamples - done);
        if (! transportStopped && done + n > musicalSamples)
            n = (int) juce::jmax ((juce::int64) 1, musicalSamples - done);

        juce::AudioBuffer<float> view (block.getArrayOfWritePointers(), 2, n);
        engine.processOffline (view, n);

        peak = juce::jmax (peak, view.getMagnitude (0, n));
        mixWriter->writeFromAudioSampleBuffer (view, 0, n);

        for (auto& stem : stems)
            if (auto* insert = engine.getInsertNode (stem.insert))
                stem.writer->writeFromAudioSampleBuffer (insert->getBuffer(), 0, n);

        done += n;
        progress = (float) done / (float) totalSamples * 0.9f;

        if (! transportStopped && done >= musicalSamples)
        {
            engine.stopOfflineTransport();
            transportStopped = true;
        }
    }

    mixWriter.reset();
    stems.clear();

    auto result = convertFinal (tempFile, peak);
    tempFile.deleteFile();
    return result;
}

juce::Result Renderer::convertFinal (const juce::File& temp, float peak)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (new juce::FileInputStream (temp), true));
    if (reader == nullptr)
        return juce::Result::fail ("Render failed: temporary file is unreadable.");

    auto format = createFormat (settings.format);
    const int bits = bitDepthsFor (settings.format).contains (settings.bitDepth) ? settings.bitDepth : bitDepthsFor (settings.format).getLast();
    auto writer = openWriter (settings.output, *format, bits == 32, bits);
    if (writer == nullptr)
        return juce::Result::fail ("Cannot write " + settings.output.getFullPathName());

    const float gain = (settings.normalize && peak > 1.0e-6f) ? juce::Decibels::decibelsToGain (-0.3f) / peak : 1.0f;
    const bool applyDither = settings.dither && bits == 16;
    const float lsb = 1.0f / 32768.0f;
    juce::Random random (1234);

    juce::AudioBuffer<float> buffer (2, 8192);
    const auto total = reader->lengthInSamples;

    for (juce::int64 pos = 0; pos < total;)
    {
        if (threadShouldExit())
            return juce::Result::fail ("Render cancelled.");

        const int n = (int) juce::jmin ((juce::int64) buffer.getNumSamples(), total - pos);
        reader->read (&buffer, 0, n, pos, true, true);
        buffer.applyGain (0, n, gain);

        if (applyDither)
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                for (int i = 0; i < n; ++i)
                    d[i] += (random.nextFloat() - random.nextFloat()) * lsb;
            }

        if (! writer->writeFromAudioSampleBuffer (buffer, 0, n))
            return juce::Result::fail ("Disk write failed while rendering.");

        pos += n;
        progress = 0.9f + 0.1f * (float) pos / (float) juce::jmax ((juce::int64) 1, total);
    }

    return juce::Result::ok();
}
}

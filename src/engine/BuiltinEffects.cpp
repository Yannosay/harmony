#include "engine/BuiltinEffects.h"

namespace harmony::engine::builtin
{
namespace
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
    using Array6 = std::array<float, 6>;
    using IIRCoeffs = juce::dsp::IIR::ArrayCoefficients<float>;

    enum class Unit { decibels, hertz, milliseconds, percent, ratio, plain, pan };

    juce::String formatValue (float v, Unit unit)
    {
        switch (unit)
        {
            case Unit::decibels:     return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB";
            case Unit::hertz:        return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : juce::String (v, v < 100.0f ? 1 : 0) + " Hz";
            case Unit::milliseconds: return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (v, v < 10.0f ? 1 : 0) + " ms";
            case Unit::percent:      return juce::String (v, 0) + "%";
            case Unit::ratio:        return v >= 19.95f ? juce::String::fromUTF8 ("\xe2\x88\x9e:1") : juce::String (v, 1) + ":1";
            case Unit::pan:          return std::abs (v) < 0.5f ? juce::String ("C") : juce::String (std::abs (v), 0) + (v < 0.0f ? " L" : " R");
            case Unit::plain:        break;
        }
        return juce::String (v, 2);
    }

    std::unique_ptr<juce::RangedAudioParameter> number (const juce::String& id, const juce::String& name, float min, float max,
                                                        float def, Unit unit, bool logarithmic = false)
    {
        juce::NormalisableRange<float> range (min, max);
        if (logarithmic)
            range.setSkewForCentre (std::sqrt (juce::jmax (0.001f, min) * max));

        return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([unit] (float v, int) { return formatValue (v, unit); }));
    }

    std::unique_ptr<juce::RangedAudioParameter> choice (const juce::String& id, const juce::String& name, const juce::StringArray& items, int def)
    {
        return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def);
    }

    std::unique_ptr<juce::RangedAudioParameter> toggle (const juce::String& id, const juce::String& name, bool def)
    {
        return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def);
    }

    float onePoleCoefficient (double sampleRate, double cutoff)
    {
        return (float) std::exp (-juce::MathConstants<double>::twoPi * juce::jmin (cutoff, sampleRate * 0.49) / sampleRate);
    }

    float timeCoefficient (double sampleRate, double milliseconds)
    {
        return (float) std::exp (-1.0 / juce::jmax (1.0, milliseconds * 0.001 * sampleRate));
    }

    void applyMix (juce::AudioBuffer<float>& wet, const juce::AudioBuffer<float>& dry, int numSamples, float mix)
    {
        if (mix >= 0.999f)
            return;

        for (int ch = 0; ch < 2; ++ch)
        {
            auto* w = wet.getWritePointer (ch);
            const auto* d = dry.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                w[i] = d[i] + (w[i] - d[i]) * mix;
        }
    }

    class StereoBiquad
    {
    public:
        void prepare() { reset(); }
        void reset() { for (auto& s : state) s = {}; }
        void set (const Array6& c) noexcept
        {
            const float a0 = c[3] != 0.0f ? c[3] : 1.0f;
            b0 = c[0] / a0; b1 = c[1] / a0; b2 = c[2] / a0; a1 = c[4] / a0; a2 = c[5] / a0;
        }

        float process (int ch, float x) noexcept
        {
            auto& s = state[(size_t) ch];
            const float y = b0 * x + s[0];
            s[0] = b1 * x - a1 * y + s[1];
            s[1] = b2 * x - a2 * y;
            return y;
        }

        void snapToZero() noexcept
        {
            for (auto& s : state)
                for (auto& v : s)
                    if (std::abs (v) < 1.0e-15f)
                        v = 0.0f;
        }

        static double magnitude (const Array6& c, double frequency, double sampleRate)
        {
            const std::complex<double> z = std::polar (1.0, -juce::MathConstants<double>::twoPi * frequency / sampleRate);
            const auto num = (double) c[0] + (double) c[1] * z + (double) c[2] * z * z;
            const auto den = (double) c[3] + (double) c[4] * z + (double) c[5] * z * z;
            return std::abs (num / den);
        }

    private:
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        std::array<std::array<float, 2>, 2> state {};
    };

    class Equalizer final : public Effect
    {
    public:
        explicit Equalizer (const Info& i) : Effect (i, layout()) {}

        static Layout layout()
        {
            Layout l;
            l.add (number ("lowCut", "Low cut", 20.0f, 1000.0f, 20.0f, Unit::hertz, true));
            l.add (number ("lowFreq", "Low freq", 30.0f, 600.0f, 120.0f, Unit::hertz, true));
            l.add (number ("lowGain", "Low gain", -18.0f, 18.0f, 0.0f, Unit::decibels));
            l.add (number ("midFreq", "Mid freq", 150.0f, 10000.0f, 1000.0f, Unit::hertz, true));
            l.add (number ("midGain", "Mid gain", -18.0f, 18.0f, 0.0f, Unit::decibels));
            l.add (number ("midQ", "Mid Q", 0.3f, 8.0f, 0.9f, Unit::plain, true));
            l.add (number ("highFreq", "High freq", 1500.0f, 16000.0f, 8000.0f, Unit::hertz, true));
            l.add (number ("highGain", "High gain", -18.0f, 18.0f, 0.0f, Unit::decibels));
            l.add (number ("highCut", "High cut", 1000.0f, 20000.0f, 20000.0f, Unit::hertz, true));
            l.add (number ("output", "Output", -24.0f, 24.0f, 0.0f, Unit::decibels));
            return l;
        }

        bool hasResponseCurve() const override { return true; }

        void computeResponse (const std::vector<double>& frequencies, std::vector<float>& decibels) const override
        {
            const auto bands = design (currentSampleRate);
            decibels.resize (frequencies.size());
            for (size_t i = 0; i < frequencies.size(); ++i)
            {
                double m = juce::Decibels::decibelsToGain ((double) param ("output"));
                for (size_t b = 0; b < bands.size(); ++b)
                    if (bandActive (b))
                        m *= StereoBiquad::magnitude (bands[b], frequencies[i], currentSampleRate);
                decibels[i] = (float) juce::Decibels::gainToDecibels (m, -60.0);
            }
        }

    private:
        static constexpr size_t numBands = 7;

        bool bandActive (size_t band) const
        {
            if (band <= 1) return param ("lowCut") > 20.5f;
            if (band >= 5) return param ("highCut") < 19900.0f;
            return true;
        }

        std::array<Array6, numBands> design (double sr) const
        {
            const float nyquistSafe = (float) sr * 0.45f;
            auto clampF = [nyquistSafe] (float f) { return juce::jlimit (10.0f, nyquistSafe, f); };
            return {
                IIRCoeffs::makeHighPass (sr, clampF (param ("lowCut")), 0.5412f),
                IIRCoeffs::makeHighPass (sr, clampF (param ("lowCut")), 1.3066f),
                IIRCoeffs::makeLowShelf (sr, clampF (param ("lowFreq")), 0.71f, juce::Decibels::decibelsToGain (param ("lowGain"))),
                IIRCoeffs::makePeakFilter (sr, clampF (param ("midFreq")), param ("midQ"), juce::Decibels::decibelsToGain (param ("midGain"))),
                IIRCoeffs::makeHighShelf (sr, clampF (param ("highFreq")), 0.71f, juce::Decibels::decibelsToGain (param ("highGain"))),
                IIRCoeffs::makeLowPass (sr, clampF (param ("highCut")), 0.5412f),
                IIRCoeffs::makeLowPass (sr, clampF (param ("highCut")), 1.3066f)
            };
        }

        void prepareEffect (const juce::dsp::ProcessSpec&) override
        {
            for (auto& f : filters)
                f.prepare();
            lastSignature = {};
            output.reset (currentSampleRate, 0.03);
            output.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (param ("output")));
        }

        void processEffect (juce::AudioBuffer<float>& buffer, int numSamples) override
        {
            std::array<float, 10> signature {};
            const char* ids[] = { "lowCut", "lowFreq", "lowGain", "midFreq", "midGain", "midQ", "highFreq", "highGain", "highCut", "output" };
            for (size_t i = 0; i < signature.size(); ++i)
                signature[i] = param (ids[i]);

            if (signature != lastSignature)
            {
                const auto bands = design (currentSampleRate);
                for (size_t b = 0; b < numBands; ++b)
                {
                    filters[b].set (bands[b]);
                    active[b] = bandActive (b);
                }
                lastSignature = signature;
            }

            output.setTargetValue (juce::Decibels::decibelsToGain (param ("output")));

            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                for (size_t b = 0; b < numBands; ++b)
                {
                    if (! active[b])
                        continue;
                    for (int i = 0; i < numSamples; ++i)
                        d[i] = filters[b].process (ch, d[i]);
                }
            }

            for (auto& f : filters)
                f.snapToZero();

            for (int i = 0; i < numSamples; ++i)
            {
                const float g = output.getNextValue();
                buffer.getWritePointer (0)[i] *= g;
                buffer.getWritePointer (1)[i] *= g;
            }
        }

        std::array<StereoBiquad, numBands> filters;
        std::array<bool, numBands> active {};
        std::array<float, 10> lastSignature {};
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> output;
    };

    class Compressor final : public Effect
    {
    public:
        explicit Compressor (const Info& i) : Effect (i, layout()) {}

        static Layout layout()
        {
            Layout l;
            l.add (number ("threshold", "Threshold", -60.0f, 0.0f, -18.0f, Unit::decibels));
            l.add (number ("ratio", "Ratio", 1.0f, 20.0f, 4.0f, Unit::ratio, true));
            l.add (number ("attack", "Attack", 0.1f, 100.0f, 10.0f, Unit::milliseconds, true));
            l.add (number ("release", "Release", 10.0f, 1500.0f, 120.0f, Unit::milliseconds, true));
            l.add (number ("knee", "Knee", 0.0f, 24.0f, 6.0f, Unit::decibels));
            l.add (number ("makeup", "Makeup", 0.0f, 24.0f, 0.0f, Unit::decibels));
            l.add (number ("mix", "Mix", 0.0f, 100.0f, 100.0f, Unit::percent));
            return l;
        }

    private:
        void prepareEffect (const juce::dsp::ProcessSpec& spec) override
        {
            dry.setSize (2, (int) spec.maximumBlockSize);
            envelope = 0.0f;
        }

        void processEffect (juce::AudioBuffer<float>& buffer, int numSamples) override
        {
            const float threshold = param ("threshold");
            const float slope = 1.0f / param ("ratio") - 1.0f;
            const float knee = param ("knee");
            const float attack = timeCoefficient (currentSampleRate, param ("attack"));
            const float release = timeCoefficient (currentSampleRate, param ("release"));
            const float makeup = param ("makeup");
            const float mix = param ("mix") * 0.01f;

            for (int ch = 0; ch < 2; ++ch)
                dry.copyFrom (ch, 0, buffer, ch, 0, numSamples);

            auto* l = buffer.getWritePointer (0);
            auto* r = buffer.getWritePointer (1);
            float deepest = 0.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                const float level = juce::Decibels::gainToDecibels (juce::jmax (std::abs (l[i]), std::abs (r[i])), -120.0f);
                const float over = level - threshold;
                float target = 0.0f;

                if (knee > 0.0f && std::abs (over) * 2.0f <= knee)
                    target = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
                else if (over > 0.0f)
                    target = slope * over;

                const float coeff = target < envelope ? attack : release;
                envelope = target + coeff * (envelope - target);
                deepest = juce::jmin (deepest, envelope);

                const float g = juce::Decibels::decibelsToGain (envelope + makeup);
                l[i] *= g;
                r[i] *= g;
            }

            reduction.store (deepest, std::memory_order_relaxed);
            applyMix (buffer, dry, numSamples, mix);
        }

        juce::AudioBuffer<float> dry;
        float envelope = 0.0f;
    };

    class Limiter final : public Effect
    {
    public:
        explicit Limiter (const Info& i) : Effect (i, layout()) {}

        static Layout layout()
        {
            Layout l;
            l.add (number ("gain", "Input gain", 0.0f, 24.0f, 0.0f, Unit::decibels));
            l.add (number ("ceiling", "Ceiling", -12.0f, 0.0f, -0.3f, Unit::decibels));
            l.add (number ("release", "Release", 1.0f, 800.0f, 80.0f, Unit::milliseconds, true));
            return l;
        }

    private:
        static constexpr double lookaheadSeconds = 0.0015;

        void prepareEffect (const juce::dsp::ProcessSpec& spec) override
        {
            lookahead = juce::jmax (1, (int) std::round (spec.sampleRate * lookaheadSeconds));
            delay.setSize (2, lookahead + 1);
            delay.clear();
            peaks.assign ((size_t) lookahead + 1, 1.0f);
            writeIndex = 0;
            gain = 1.0f;
            setLatencySamples (lookahead);
        }

        void processEffect (juce::AudioBuffer<float>& buffer, int numSamples) override
        {
            const float input = juce::Decibels::decibelsToGain (param ("gain"));
            const float ceiling = juce::Decibels::decibelsToGain (param ("ceiling"));
            const float release = timeCoefficient (currentSampleRate, param ("release"));
            const float attackStep = 1.0f / (float) lookahead;
            const int size = delay.getNumSamples();

            auto* l = buffer.getWritePointer (0);
            auto* r = buffer.getWritePointer (1);
            auto* dl = delay.getWritePointer (0);
            auto* dr = delay.getWritePointer (1);
            float deepest = 1.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                const float inL = l[i] * input, inR = r[i] * input;
                const float peak = juce::jmax (std::abs (inL), std::abs (inR));
                peaks[(size_t) writeIndex] = peak > ceiling ? ceiling / peak : 1.0f;

                float required = 1.0f;
                for (auto p : peaks)
                    required = juce::jmin (required, p);

                if (required < gain)
                    gain = juce::jmax (required, gain - attackStep);
                else
                    gain = required + release * (gain - required);

                dl[writeIndex] = inL;
                dr[writeIndex] = inR;
                const int readIndex = (writeIndex + 1) % size;
                l[i] = juce::jlimit (-ceiling, ceiling, dl[readIndex] * gain);
                r[i] = juce::jlimit (-ceiling, ceiling, dr[readIndex] * gain);
                writeIndex = readIndex;
                deepest = juce::jmin (deepest, gain);
            }

            reduction.store (juce::Decibels::gainToDecibels (deepest, -60.0f), std::memory_order_relaxed);
        }

        juce::AudioBuffer<float> delay;
        std::vector<float> peaks;
        int lookahead = 64, writeIndex = 0;
        float gain = 1.0f;
    };

    class Reverb final : public Effect
    {
    public:
        explicit Reverb (const Info& i) : Effect (i, layout()) {}

        static Layout layout()
        {
            Layout l;
            l.add (number ("size", "Size", 0.0f, 100.0f, 55.0f, Unit::percent));
            l.add (number ("damping", "Damping", 0.0f, 100.0f, 45.0f, Unit::percent));
            l.add (number ("predelay", "Pre-delay", 0.0f, 200.0f, 12.0f, Unit::milliseconds));
            l.add (number ("lowCut", "Low cut", 20.0f, 1000.0f, 180.0f, Unit::hertz, true));
            l.add (number ("width", "Width", 0.0f, 100.0f, 100.0f, Unit::percent));
            l.add (number ("mix", "Mix", 0.0f, 100.0f, 25.0f, Unit::percent));
            return l;
        }

        double getTailLengthSeconds() const override { return 10.0; }

    private:
        void prepareEffect (const juce::dsp::ProcessSpec& spec) override
        {
            reverb.prepare (spec);
            reverb.reset();
            preDelay.prepare (spec);
            preDelay.setMaximumDelayInSamples ((int) (spec.sampleRate * 0.25) + 1);
            preDelay.reset();
            wet.setSize (2, (int) spec.maximumBlockSize);
            lowCut.reset();
        }

        void processEffect (juce::AudioBuffer<float>& buffer, int numSamples) override
        {
            juce::dsp::Reverb::Parameters p;
            p.roomSize = param ("size") * 0.01f;
            p.damping = param ("damping") * 0.01f;
            p.width = param ("width") * 0.01f;
            p.wetLevel = 1.0f;
            p.dryLevel = 0.0f;
            reverb.setParameters (p);
            preDelay.setDelay ((float) (param ("predelay") * 0.001 * currentSampleRate));
            lowCut.set (IIRCoeffs::makeHighPass (currentSampleRate, juce::jlimit (10.0f, (float) currentSampleRate * 0.45f, param ("lowCut")), 0.707f));

            for (int ch = 0; ch < 2; ++ch)
            {
                auto* src = buffer.getReadPointer (ch);
                auto* dst = wet.getWritePointer (ch);
                for (int i = 0; i < numSamples; ++i)
                {
                    preDelay.pushSample (ch, lowCut.process (ch, src[i]));
                    dst[i] = preDelay.popSample (ch);
                }
            }
            lowCut.snapToZero();

            juce::dsp::AudioBlock<float> block (wet.getArrayOfWritePointers(), 2, (size_t) numSamples);
            reverb.process (juce::dsp::ProcessContextReplacing<float> (block));

            const float mix = param ("mix") * 0.01f;
            const float dryGain = std::cos (mix * juce::MathConstants<float>::halfPi);
            const float wetGain = std::sin (mix * juce::MathConstants<float>::halfPi);
            for (int ch = 0; ch < 2; ++ch)
            {
                buffer.applyGain (ch, 0, numSamples, dryGain);
                buffer.addFrom (ch, 0, wet, ch, 0, numSamples, wetGain);
            }
        }

        juce::dsp::Reverb reverb;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> preDelay { 48000 };
        StereoBiquad lowCut;
        juce::AudioBuffer<float> wet;
    };

    class Delay final : public Effect
    {
    public:
        explicit Delay (const Info& i) : Effect (i, layout()) {}

        static const juce::StringArray& divisions()
        {
            static const juce::StringArray names { "1/1", "1/2", "1/2 dotted", "1/4", "1/4 dotted", "1/4 triplet", "1/8", "1/8 dotted",
                                                   "1/8 triplet", "1/16", "1/16 dotted", "1/16 triplet", "1/32" };
            return names;
        }

        static double beatsFor (int division)
        {
            static const double beats[] = { 4.0, 2.0, 3.0, 1.0, 1.5, 2.0 / 3.0, 0.5, 0.75, 1.0 / 3.0, 0.25, 0.375, 1.0 / 6.0, 0.125 };
            return beats[juce::jlimit (0, (int) std::size (beats) - 1, division)];
        }

        static Layout layout()
        {
            Layout l;
            l.add (toggle ("sync", "Tempo sync", true));
            l.add (choice ("division", "Division", divisions(), 7));
            l.add (number ("time", "Time", 1.0f, 2000.0f, 375.0f, Unit::milliseconds, true));
            l.add (number ("feedback", "Feedback", 0.0f, 95.0f, 38.0f, Unit::percent));
            l.add (number ("tone", "Tone", 500.0f, 20000.0f, 7000.0f, Unit::hertz, true));
            l.add (toggle ("pingpong", "Ping-pong", true));
            l.add (number ("mix", "Mix", 0.0f, 100.0f, 25.0f, Unit::percent));
            return l;
        }

        double getTailLengthSeconds() const override { return 6.0; }

    private:
        void prepareEffect (const juce::dsp::ProcessSpec& spec) override
        {
            line.prepare (spec);
            line.setMaximumDelayInSamples ((int) (spec.sampleRate * 4.1));
            line.reset();
            delaySamples.reset (spec.sampleRate, 0.12);
            delaySamples.setCurrentAndTargetValue ((float) targetDelay());
            toneState = { 0.0f, 0.0f };
        }

        double targetDelay() const
        {
            const double ms = param ("sync") > 0.5f ? beatsFor ((int) param ("division")) * 60000.0 / getHostBpm()
                                                    : (double) param ("time");
            return juce::jlimit (1.0, currentSampleRate * 4.0, ms * 0.001 * currentSampleRate);
        }

        void processEffect (juce::AudioBuffer<float>& buffer, int numSamples) override
        {
            delaySamples.setTargetValue ((float) targetDelay());
            const float feedback = param ("feedback") * 0.01f;
            const float toneCoeff = onePoleCoefficient (currentSampleRate, param ("tone"));
            const bool pingPong = param ("pingpong") > 0.5f;
            const float mix = param ("mix") * 0.01f;

            auto* l = buffer.getWritePointer (0);
            auto* r = buffer.getWritePointer (1);

            for (int i = 0; i < numSamples; ++i)
            {
                line.setDelay (delaySamples.getNextValue());
                float tapL = line.popSample (0);
                float tapR = line.popSample (1);

                toneState[0] = tapL + toneCoeff * (toneState[0] - tapL);
                toneState[1] = tapR + toneCoeff * (toneState[1] - tapR);
                tapL = toneState[0];
                tapR = toneState[1];

                if (pingPong)
                {
                    line.pushSample (0, (l[i] + r[i]) * 0.5f + tapR * feedback);
                    line.pushSample (1, tapL * feedback);
                }
                else
                {
                    line.pushSample (0, l[i] + tapL * feedback);
                    line.pushSample (1, r[i] + tapR * feedback);
                }

                l[i] += tapL * mix;
                r[i] += tapR * mix;
            }

            for (auto& s : toneState)
                if (std::abs (s) < 1.0e-15f)
                    s = 0.0f;
        }

        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> line { 192000 };
        juce::SmoothedValue<float> delaySamples;
        std::array<float, 2> toneState {};
    };

    class Saturator final : public Effect
    {
    public:
        explicit Saturator (const Info& i) : Effect (i, layout()) {}

        static Layout layout()
        {
            Layout l;
            l.add (choice ("mode", "Mode", { "Soft", "Warm", "Hard", "Fold" }, 0));
            l.add (number ("drive", "Drive", 0.0f, 36.0f, 8.0f, Unit::decibels));
            l.add (number ("tone", "Tone", 1000.0f, 20000.0f, 16000.0f, Unit::hertz, true));
            l.add (number ("output", "Output", -24.0f, 12.0f, 0.0f, Unit::decibels));
            l.add (number ("mix", "Mix", 0.0f, 100.0f, 100.0f, Unit::percent));
            return l;
        }

    private:
        static float shape (int mode, float x) noexcept
        {
            switch (mode)
            {
                case 1:  return x >= 0.0f ? std::tanh (x) : std::tanh (x * 0.7f) / 0.7f * 0.9f;
                case 2:  return juce::jlimit (-1.0f, 1.0f, x);
                case 3:  return std::sin (x * juce::MathConstants<float>::halfPi);
                default: return std::tanh (x);
            }
        }

        void prepareEffect (const juce::dsp::ProcessSpec& spec) override
        {
            oversampling.initProcessing (spec.maximumBlockSize);
            oversampling.reset();
            setLatencySamples ((int) std::round (oversampling.getLatencyInSamples()));
            dry.setSize (2, (int) spec.maximumBlockSize);
            dryDelay.prepare (spec);
            dryDelay.setMaximumDelayInSamples (64);
            dryDelay.setDelay ((float) oversampling.getLatencyInSamples());
            dryDelay.reset();
            toneState = { 0.0f, 0.0f };
            dcState = { 0.0f, 0.0f, 0.0f, 0.0f };
        }

        void processEffect (juce::AudioBuffer<float>& buffer, int numSamples) override
        {
            const int mode = (int) param ("mode");
            const float drive = juce::Decibels::decibelsToGain (param ("drive"));
            const float compensation = 1.0f / std::sqrt (drive);
            const float output = juce::Decibels::decibelsToGain (param ("output"));
            const float mix = param ("mix") * 0.01f;
            const float toneCoeff = onePoleCoefficient (currentSampleRate, param ("tone"));
            const float dcCoeff = onePoleCoefficient (currentSampleRate, 8.0);

            for (int ch = 0; ch < 2; ++ch)
            {
                auto* src = buffer.getReadPointer (ch);
                auto* dst = dry.getWritePointer (ch);
                for (int i = 0; i < numSamples; ++i)
                {
                    dryDelay.pushSample (ch, src[i]);
                    dst[i] = dryDelay.popSample (ch);
                }
            }

            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) numSamples);
            auto up = oversampling.processSamplesUp (block);
            for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            {
                auto* d = up.getChannelPointer (ch);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                    d[i] = shape (mode, d[i] * drive) * compensation;
            }
            oversampling.processSamplesDown (block);

            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                for (int i = 0; i < numSamples; ++i)
                {
                    const float x = d[i];
                    auto& lp = toneState[(size_t) ch];
                    lp = x + toneCoeff * (lp - x);
                    auto& prevIn = dcState[(size_t) ch * 2];
                    auto& prevOut = dcState[(size_t) ch * 2 + 1];
                    const float hp = lp - prevIn + dcCoeff * prevOut;
                    prevIn = lp;
                    prevOut = hp;
                    d[i] = hp * output;
                }
            }

            applyMix (buffer, dry, numSamples, mix);
        }

        juce::dsp::Oversampling<float> oversampling { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Thiran> dryDelay { 64 };
        juce::AudioBuffer<float> dry;
        std::array<float, 2> toneState {};
        std::array<float, 4> dcState {};
    };

    class Chorus final : public Effect
    {
    public:
        explicit Chorus (const Info& i) : Effect (i, layout()) {}

        static Layout layout()
        {
            Layout l;
            l.add (number ("rate", "Rate", 0.05f, 8.0f, 0.8f, Unit::hertz, true));
            l.add (number ("depth", "Depth", 0.0f, 100.0f, 35.0f, Unit::percent));
            l.add (number ("delay", "Delay", 1.0f, 40.0f, 8.0f, Unit::milliseconds));
            l.add (number ("feedback", "Feedback", -90.0f, 90.0f, 0.0f, Unit::percent));
            l.add (number ("mix", "Mix", 0.0f, 100.0f, 50.0f, Unit::percent));
            return l;
        }

    private:
        void prepareEffect (const juce::dsp::ProcessSpec& spec) override
        {
            chorus.prepare (spec);
            chorus.reset();
        }

        void processEffect (juce::AudioBuffer<float>& buffer, int numSamples) override
        {
            chorus.setRate (param ("rate"));
            chorus.setDepth (param ("depth") * 0.01f);
            chorus.setCentreDelay (param ("delay"));
            chorus.setFeedback (param ("feedback") * 0.01f);
            chorus.setMix (param ("mix") * 0.01f);
            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) numSamples);
            chorus.process (juce::dsp::ProcessContextReplacing<float> (block));
        }

        juce::dsp::Chorus<float> chorus;
    };

    class Phaser final : public Effect
    {
    public:
        explicit Phaser (const Info& i) : Effect (i, layout()) {}

        static Layout layout()
        {
            Layout l;
            l.add (number ("rate", "Rate", 0.02f, 10.0f, 0.4f, Unit::hertz, true));
            l.add (number ("depth", "Depth", 0.0f, 100.0f, 60.0f, Unit::percent));
            l.add (number ("centre", "Centre", 100.0f, 8000.0f, 1100.0f, Unit::hertz, true));
            l.add (number ("feedback", "Feedback", -90.0f, 90.0f, 40.0f, Unit::percent));
            l.add (number ("mix", "Mix", 0.0f, 100.0f, 50.0f, Unit::percent));
            return l;
        }

    private:
        void prepareEffect (const juce::dsp::ProcessSpec& spec) override
        {
            phaser.prepare (spec);
            phaser.reset();
        }

        void processEffect (juce::AudioBuffer<float>& buffer, int numSamples) override
        {
            phaser.setRate (param ("rate"));
            phaser.setDepth (param ("depth") * 0.01f);
            phaser.setCentreFrequency (param ("centre"));
            phaser.setFeedback (param ("feedback") * 0.01f);
            phaser.setMix (param ("mix") * 0.01f);
            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) numSamples);
            phaser.process (juce::dsp::ProcessContextReplacing<float> (block));
        }

        juce::dsp::Phaser<float> phaser;
    };

    class Filter final : public Effect
    {
    public:
        explicit Filter (const Info& i) : Effect (i, layout()) {}

        static Layout layout()
        {
            Layout l;
            l.add (choice ("type", "Type", { "Low pass", "High pass", "Band pass" }, 0));
            l.add (number ("cutoff", "Cutoff", 20.0f, 20000.0f, 20000.0f, Unit::hertz, true));
            l.add (number ("resonance", "Resonance", 0.5f, 12.0f, 0.71f, Unit::plain, true));
            l.add (number ("drive", "Drive", 0.0f, 24.0f, 0.0f, Unit::decibels));
            l.add (number ("mix", "Mix", 0.0f, 100.0f, 100.0f, Unit::percent));
            return l;
        }

        bool hasResponseCurve() const override { return true; }

        void computeResponse (const std::vector<double>& frequencies, std::vector<float>& decibels) const override
        {
            const int type = (int) param ("type");
            const double fc = param ("cutoff");
            const double q = param ("resonance");
            decibels.resize (frequencies.size());

            for (size_t i = 0; i < frequencies.size(); ++i)
            {
                const double w = frequencies[i] / fc;
                const std::complex<double> s (0.0, w);
                const auto den = s * s + s / q + 1.0;
                const auto num = type == 0 ? std::complex<double> (1.0, 0.0) : type == 1 ? s * s : s / q;
                decibels[i] = (float) juce::Decibels::gainToDecibels (std::abs (num / den), -60.0);
            }
        }

    private:
        void prepareEffect (const juce::dsp::ProcessSpec& spec) override
        {
            filter.prepare (spec);
            filter.reset();
            cutoff.reset (spec.sampleRate, 0.03);
            cutoff.setCurrentAndTargetValue (param ("cutoff"));
            dry.setSize (2, (int) spec.maximumBlockSize);
        }

        void processEffect (juce::AudioBuffer<float>& buffer, int numSamples) override
        {
            using Type = juce::dsp::StateVariableTPTFilterType;
            const int type = (int) param ("type");
            filter.setType (type == 0 ? Type::lowpass : type == 1 ? Type::highpass : Type::bandpass);
            filter.setResonance (param ("resonance"));
            cutoff.setTargetValue (juce::jlimit (20.0f, (float) currentSampleRate * 0.45f, param ("cutoff")));
            const float drive = juce::Decibels::decibelsToGain (param ("drive"));
            const float mix = param ("mix") * 0.01f;

            for (int ch = 0; ch < 2; ++ch)
                dry.copyFrom (ch, 0, buffer, ch, 0, numSamples);

            auto* l = buffer.getWritePointer (0);
            auto* r = buffer.getWritePointer (1);
            for (int i = 0; i < numSamples; ++i)
            {
                if ((i & 15) == 0)
                    filter.setCutoffFrequency (cutoff.getNextValue());
                else
                    cutoff.getNextValue();

                float inL = l[i], inR = r[i];
                if (drive > 1.001f)
                {
                    inL = std::tanh (inL * drive) / std::sqrt (drive);
                    inR = std::tanh (inR * drive) / std::sqrt (drive);
                }
                l[i] = filter.processSample (0, inL);
                r[i] = filter.processSample (1, inR);
            }
            filter.snapToZero();
            applyMix (buffer, dry, numSamples, mix);
        }

        juce::dsp::StateVariableTPTFilter<float> filter;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoff;
        juce::AudioBuffer<float> dry;
    };

    class Utility final : public Effect
    {
    public:
        explicit Utility (const Info& i) : Effect (i, layout()) {}

        static Layout layout()
        {
            Layout l;
            l.add (number ("gain", "Gain", -48.0f, 24.0f, 0.0f, Unit::decibels));
            l.add (number ("pan", "Pan", -100.0f, 100.0f, 0.0f, Unit::pan));
            l.add (number ("width", "Width", 0.0f, 200.0f, 100.0f, Unit::percent));
            l.add (toggle ("mono", "Mono", false));
            l.add (toggle ("invertL", "Invert left", false));
            l.add (toggle ("invertR", "Invert right", false));
            l.add (toggle ("swap", "Swap channels", false));
            return l;
        }

    private:
        void prepareEffect (const juce::dsp::ProcessSpec& spec) override
        {
            left.reset (spec.sampleRate, 0.03);
            right.reset (spec.sampleRate, 0.03);
            width.reset (spec.sampleRate, 0.03);
            left.setCurrentAndTargetValue (targetLeft());
            right.setCurrentAndTargetValue (targetRight());
            width.setCurrentAndTargetValue (targetWidth());
        }

        float panGain (bool isLeft) const
        {
            const float pan = param ("pan") * 0.01f;
            const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            return juce::MathConstants<float>::sqrt2 * (isLeft ? std::cos (angle) : std::sin (angle));
        }

        float targetLeft() const  { return juce::Decibels::decibelsToGain (param ("gain"), -48.0f) * panGain (true) * (param ("invertL") > 0.5f ? -1.0f : 1.0f); }
        float targetRight() const { return juce::Decibels::decibelsToGain (param ("gain"), -48.0f) * panGain (false) * (param ("invertR") > 0.5f ? -1.0f : 1.0f); }
        float targetWidth() const { return param ("mono") > 0.5f ? 0.0f : param ("width") * 0.01f; }

        void processEffect (juce::AudioBuffer<float>& buffer, int numSamples) override
        {
            left.setTargetValue (targetLeft());
            right.setTargetValue (targetRight());
            width.setTargetValue (targetWidth());
            const bool swap = param ("swap") > 0.5f;

            auto* l = buffer.getWritePointer (0);
            auto* r = buffer.getWritePointer (1);
            for (int i = 0; i < numSamples; ++i)
            {
                float a = swap ? r[i] : l[i];
                float b = swap ? l[i] : r[i];
                const float mid = (a + b) * 0.5f;
                const float side = (a - b) * 0.5f * width.getNextValue();
                l[i] = (mid + side) * left.getNextValue();
                r[i] = (mid - side) * right.getNextValue();
            }
        }

        juce::SmoothedValue<float> left, right, width;
    };

    class Gate final : public Effect
    {
    public:
        explicit Gate (const Info& i) : Effect (i, layout()) {}

        static Layout layout()
        {
            Layout l;
            l.add (number ("threshold", "Threshold", -80.0f, 0.0f, -45.0f, Unit::decibels));
            l.add (number ("range", "Range", -80.0f, 0.0f, -60.0f, Unit::decibels));
            l.add (number ("attack", "Attack", 0.05f, 50.0f, 1.0f, Unit::milliseconds, true));
            l.add (number ("hold", "Hold", 0.0f, 500.0f, 30.0f, Unit::milliseconds));
            l.add (number ("release", "Release", 5.0f, 2000.0f, 150.0f, Unit::milliseconds, true));
            return l;
        }

    private:
        void prepareEffect (const juce::dsp::ProcessSpec&) override
        {
            gain = 1.0f;
            holdCounter = 0;
        }

        void processEffect (juce::AudioBuffer<float>& buffer, int numSamples) override
        {
            const float threshold = juce::Decibels::decibelsToGain (param ("threshold"));
            const float floorGain = juce::Decibels::decibelsToGain (param ("range"), -80.0f);
            const float attack = timeCoefficient (currentSampleRate, param ("attack"));
            const float release = timeCoefficient (currentSampleRate, param ("release"));
            const int holdSamples = (int) (param ("hold") * 0.001f * currentSampleRate);

            auto* l = buffer.getWritePointer (0);
            auto* r = buffer.getWritePointer (1);
            float lowest = 1.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                const float level = juce::jmax (std::abs (l[i]), std::abs (r[i]));
                float target = floorGain;
                if (level >= threshold)
                {
                    holdCounter = holdSamples;
                    target = 1.0f;
                }
                else if (holdCounter > 0)
                {
                    --holdCounter;
                    target = 1.0f;
                }

                const float coeff = target > gain ? attack : release;
                gain = target + coeff * (gain - target);
                l[i] *= gain;
                r[i] *= gain;
                lowest = juce::jmin (lowest, gain);
            }

            reduction.store (juce::Decibels::gainToDecibels (lowest, -80.0f), std::memory_order_relaxed);
        }

        float gain = 1.0f;
        int holdCounter = 0;
    };

    template <typename T>
    std::unique_ptr<juce::AudioPluginInstance> make (const Info& info)
    {
        return std::make_unique<T> (info);
    }

    struct Entry
    {
        Info info;
        std::unique_ptr<juce::AudioPluginInstance> (*factory) (const Info&);
    };

    const std::vector<Entry>& entries()
    {
        static const std::vector<Entry> list {
            { { "eq",         "Harmony EQ",         "Tone",     "Five band equaliser with steep low and high cuts" }, &make<Equalizer> },
            { { "filter",     "Harmony Filter",     "Tone",     "Resonant low, high and band pass filter made for automation" }, &make<Filter> },
            { { "compressor", "Harmony Compressor", "Dynamics", "Smooth soft-knee compressor with parallel mix" }, &make<Compressor> },
            { { "limiter",    "Harmony Limiter",    "Dynamics", "Look-ahead peak limiter for the master" }, &make<Limiter> },
            { { "gate",       "Harmony Gate",       "Dynamics", "Noise gate with hold and range" }, &make<Gate> },
            { { "saturator",  "Harmony Saturator",  "Colour",   "Oversampled saturation with four curves" }, &make<Saturator> },
            { { "reverb",     "Harmony Reverb",     "Space",    "Lush room and hall reverb with pre-delay" }, &make<Reverb> },
            { { "delay",      "Harmony Delay",      "Space",    "Tempo synced stereo and ping-pong delay" }, &make<Delay> },
            { { "chorus",     "Harmony Chorus",     "Modulation", "Classic stereo chorus" }, &make<Chorus> },
            { { "phaser",     "Harmony Phaser",     "Modulation", "Sweeping multi-stage phaser" }, &make<Phaser> },
            { { "utility",    "Harmony Utility",    "Utility",  "Gain, pan, stereo width, mono and phase tools" }, &make<Utility> }
        };
        return list;
    }

}

const std::vector<Info>& all()
{
    static const std::vector<Info> infos = []
    {
        std::vector<Info> result;
        for (auto& e : entries())
            result.push_back (e.info);
        return result;
    }();
    return infos;
}

const Info* find (const juce::String& id)
{
    for (auto& info : all())
        if (info.id == id)
            return &info;
    return nullptr;
}

juce::PluginDescription describe (const Info& info)
{
    juce::PluginDescription d;
    d.name = info.name;
    d.descriptiveName = info.description;
    d.pluginFormatName = formatName;
    d.category = info.group;
    d.manufacturerName = vendor;
    d.version = HARMONY_VERSION_STRING;
    d.fileOrIdentifier = "harmony:" + info.id;
    d.uniqueId = d.deprecatedUid = (int) juce::String ("harmony:" + info.id).hashCode();
    d.isInstrument = false;
    d.numInputChannels = 2;
    d.numOutputChannels = 2;
    d.hasSharedContainer = false;
    d.hasARAExtension = false;
    return d;
}

bool isBuiltin (const juce::PluginDescription& description)
{
    return description.pluginFormatName == formatName;
}

std::unique_ptr<juce::AudioPluginInstance> create (const juce::String& id)
{
    const auto& list = entries();
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].info.id == id)
            return list[i].factory (all()[i]);
    return nullptr;
}

Effect::Effect (const Info& i, juce::AudioProcessorValueTreeState::ParameterLayout layout)
    : juce::AudioPluginInstance (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                                  .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      info (i),
      state (*this, nullptr, "HARMONY_EFFECT", std::move (layout))
{
}

void Effect::computeResponse (const std::vector<double>& frequencies, std::vector<float>& decibels) const
{
    decibels.assign (frequencies.size(), 0.0f);
}

juce::AudioProcessorEditor* Effect::createEditor()
{
    if (editorFactory)
        if (auto* editor = editorFactory (*this))
            return editor;
    return new juce::GenericAudioProcessorEditor (*this);
}

float Effect::param (juce::StringRef id) const
{
    if (auto* value = state.getRawParameterValue (id))
        return value->load (std::memory_order_relaxed);
    jassertfalse;
    return 0.0f;
}

double Effect::getHostBpm() const
{
    if (auto* head = getPlayHead())
        if (auto position = head->getPosition())
            if (auto bpm = position->getBpm())
                return juce::jlimit (10.0, 999.0, *bpm);
    return 120.0;
}

void Effect::getStateInformation (juce::MemoryBlock& destination)
{
    auto copy = state.copyState();
    copy.setProperty ("effect", info.id, nullptr);
    if (auto xml = copy.createXml())
        copyXmlToBinary (*xml, destination);
}

void Effect::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

bool Effect::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void Effect::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    maxBlock = juce::jmax (1, maximumExpectedSamplesPerBlock);
    prepareEffect ({ currentSampleRate, (juce::uint32) maxBlock, 2 });
}

void Effect::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumChannels() < 2)
        return;

    for (int offset = 0; offset < buffer.getNumSamples(); offset += maxBlock)
    {
        const int n = juce::jmin (maxBlock, buffer.getNumSamples() - offset);
        juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, offset, n);
        processEffect (view, n);
    }
}
}

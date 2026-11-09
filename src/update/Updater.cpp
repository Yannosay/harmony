#include "update/Updater.h"
#include <juce_cryptography/juce_cryptography.h>

namespace harmony::update
{
namespace
{
    juce::URL::InputStreamOptions requestOptions (int timeoutMs)
    {
        return juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs (timeoutMs)
            .withNumRedirectsToFollow (8)
            .withExtraHeaders ("User-Agent: YannosayHarmony/" HARMONY_VERSION_STRING "\r\nAccept: application/vnd.github+json");
    }
}

juce::String platformInstallerSuffix()
{
   #if JUCE_WINDOWS
    return "-Setup-x64.exe";
   #elif JUCE_MAC
    return "-macOS.dmg";
   #else
    return "-Linux-x64.tar.gz";
   #endif
}

std::optional<ReleaseInfo> parseRelease (const juce::var& json, const juce::String& suffix)
{
    if (! json.isObject() || (bool) json["draft"] || (bool) json["prerelease"])
        return std::nullopt;

    const auto tag = json["tag_name"].toString();
    const auto version = Version::parse (tag);
    if (! version.has_value())
        return std::nullopt;

    ReleaseInfo info;
    info.version = *version;
    info.tag = tag;
    info.notes = json["body"].toString();

    if (auto* assets = json["assets"].getArray())
    {
        for (auto& asset : *assets)
        {
            const auto name = asset["name"].toString();
            const auto url = asset["browser_download_url"].toString();

            if (! url.startsWith ("https://"))
                continue;

            if (name.endsWithIgnoreCase (suffix))
            {
                info.installerName = name;
                info.installerUrl = juce::URL (url);
            }
            else if (name.equalsIgnoreCase ("SHA256SUMS.txt"))
            {
                info.checksumsUrl = juce::URL (url);
            }
        }
    }

    if (info.installerName.isEmpty() || info.checksumsUrl.isEmpty())
        return std::nullopt;

    return info;
}

std::optional<ReleaseInfo> parseManifest (const juce::var& json, const juce::String& suffix)
{
    if (! json.isObject())
        return std::nullopt;

    const auto version = Version::parse (json["version"].toString());
    const auto url = json["installer"].toString();
    const auto hash = json["sha256"].toString().toLowerCase();
    const auto name = json["file"].toString();

    if (! version.has_value() || ! url.startsWith ("https://") || ! name.endsWithIgnoreCase (suffix)
        || hash.length() != 64 || ! hash.containsOnly ("0123456789abcdef")
        || name.containsAnyOf ("/\\:") || ! url.endsWith ("/" + name))
        return std::nullopt;

    ReleaseInfo info;
    info.version = *version;
    info.tag = "v" + version->toString();
    info.installerName = name;
    info.installerUrl = juce::URL (url);
    info.sha256 = hash;
    return info;
}

juce::String findChecksum (const juce::String& checksumsText, const juce::String& fileName)
{
    for (auto line : juce::StringArray::fromLines (checksumsText))
    {
        line = line.trim();
        const auto hash = line.upToFirstOccurrenceOf (" ", false, false).trim();
        const auto name = line.fromFirstOccurrenceOf (" ", false, false).trim().trimCharactersAtStart ("*");

        if (name == fileName && hash.length() == 64 && hash.containsOnly ("0123456789abcdefABCDEF"))
            return hash.toLowerCase();
    }
    return {};
}

bool verifySha256 (const juce::File& file, const juce::String& expectedHex)
{
    if (expectedHex.length() != 64 || ! file.existsAsFile())
        return false;

    return juce::SHA256 (file).toHexString().equalsIgnoreCase (expectedHex);
}

Updater::Updater (const juce::File& downloadDirectory)
    : juce::Thread ("Harmony updater"), directory (downloadDirectory)
{
}

Updater::~Updater()
{
    stopThread (8000);
}

void Updater::check (bool downloadIfAvailable)
{
    if (isThreadRunning())
        return;

    autoDownload = downloadIfAvailable;
    startThread (juce::Thread::Priority::low);
}

juce::String Updater::getAvailableVersion() const
{
    const juce::ScopedLock sl (lock);
    return available.has_value() ? available->version.toString() : juce::String();
}

juce::String Updater::getReleaseNotes() const
{
    const juce::ScopedLock sl (lock);
    return available.has_value() ? available->notes : juce::String();
}

juce::String Updater::getError() const
{
    const juce::ScopedLock sl (lock);
    return error;
}

void Updater::setState (State newState)
{
    state = newState;
    juce::MessageManager::callAsync ([this, weak = std::weak_ptr<bool> (alive)]
    {
        if (! weak.expired() && onStateChanged)
            onStateChanged();
    });
}

void Updater::run()
{
    setState (State::checking);

    const auto release = fetchLatest();
    if (! release.has_value())
    {
        { const juce::ScopedLock sl (lock); error = "Could not reach the update server."; }
        setState (State::failed);
        return;
    }

    if (! (currentVersion() < release->version))
    {
        setState (State::upToDate);
        return;
    }

    { const juce::ScopedLock sl (lock); available = release; }

    if (! autoDownload.load())
    {
        setState (State::idle);
        return;
    }

    setState (State::downloading);
    const auto result = download (*release);

    if (result.failed())
    {
        { const juce::ScopedLock sl (lock); error = result.getErrorMessage(); }
        setState (State::failed);
        return;
    }

    setState (State::ready);
}

std::optional<ReleaseInfo> Updater::fetchLatest() const
{
    if (auto stream = juce::URL (manifestUrl).createInputStream (requestOptions (10000)))
        if (auto info = parseManifest (juce::JSON::parse (stream->readEntireStreamAsString()), platformInstallerSuffix()))
            return info;

    const juce::URL api ("https://api.github.com/repos/" + juce::String (repository) + "/releases/latest");
    if (auto stream = api.createInputStream (requestOptions (15000)))
        return parseRelease (juce::JSON::parse (stream->readEntireStreamAsString()), platformInstallerSuffix());

    return std::nullopt;
}

juce::Result Updater::download (const ReleaseInfo& release)
{
    juce::String expected = release.sha256;
    if (expected.isEmpty())
    {
        auto sumsStream = release.checksumsUrl.createInputStream (requestOptions (15000));
        if (sumsStream == nullptr)
            return juce::Result::fail ("Could not download checksums.");

        expected = findChecksum (sumsStream->readEntireStreamAsString(), release.installerName);
    }

    if (expected.isEmpty())
        return juce::Result::fail ("The release has no checksum for " + release.installerName);

    directory.createDirectory();
    const auto target = directory.getChildFile (release.installerName);

    if (! verifySha256 (target, expected))
    {
        auto in = release.installerUrl.createInputStream (requestOptions (30000));
        if (in == nullptr)
            return juce::Result::fail ("Could not download the update.");

        const auto partial = target.withFileExtension (".part");
        partial.deleteFile();
        juce::FileOutputStream out (partial);
        if (! out.openedOk())
            return juce::Result::fail ("Cannot write to " + directory.getFullPathName());

        const auto total = in->getTotalLength();
        juce::HeapBlock<char> chunk (1 << 16);
        juce::int64 received = 0;

        while (! in->isExhausted())
        {
            if (threadShouldExit())
                return juce::Result::fail ("Download cancelled.");

            const int n = in->read (chunk.get(), 1 << 16);
            if (n <= 0)
                break;

            out.write (chunk.get(), (size_t) n);
            received += n;
            if (total > 0)
                downloadProgress = (float) received / (float) total;
        }

        out.flush();
        if (out.getStatus().failed())
            return juce::Result::fail ("Disk write failed while downloading.");

        target.deleteFile();
        if (! partial.moveFileTo (target))
            return juce::Result::fail ("Could not finalise the downloaded update.");
    }

    if (! verifySha256 (target, expected))
    {
        target.deleteFile();
        return juce::Result::fail ("The downloaded update failed verification.");
    }

    const juce::ScopedLock sl (lock);
    installer = target;
    return juce::Result::ok();
}

bool Updater::installAndQuit (bool relaunch)
{
    juce::File file;
    {
        const juce::ScopedLock sl (lock);
        file = installer;
    }

    if (state.load() != State::ready || ! file.existsAsFile())
        return false;

   #if JUCE_WINDOWS
    if (! file.startAsProcess (juce::String ("/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS") + (relaunch ? " /RELAUNCH" : "")))
        return false;
   #else
    juce::ignoreUnused (relaunch);
    if (! file.startAsProcess())
        return false;
   #endif

    juce::JUCEApplicationBase::quit();
    return true;
}
}

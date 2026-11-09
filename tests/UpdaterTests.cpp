#include "update/Updater.h"

namespace harmony::update
{
class UpdaterTests final : public juce::UnitTest
{
public:
    UpdaterTests() : juce::UnitTest ("Updater", "Harmony") {}

    static juce::var release (const juce::String& tag, bool withChecksums, bool draft = false)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("tag_name", tag);
        obj->setProperty ("draft", draft);
        obj->setProperty ("prerelease", false);
        obj->setProperty ("body", "Notes");

        juce::Array<juce::var> assets;
        auto asset = [] (const juce::String& assetName)
        {
            auto* a = new juce::DynamicObject();
            a->setProperty ("name", assetName);
            a->setProperty ("browser_download_url", "https://github.com/Yannosay/harmony/releases/download/x/" + assetName);
            return juce::var (a);
        };
        assets.add (asset ("YannosayHarmony-" + tag.trimCharactersAtStart ("v") + platformInstallerSuffix()));
        if (withChecksums)
            assets.add (asset ("SHA256SUMS.txt"));
        obj->setProperty ("assets", assets);
        return juce::var (obj);
    }

    void runTest() override
    {
        beginTest ("Version parsing and ordering");
        {
            expect (Version::parse ("v1.2.3").has_value());
            expect (*Version::parse ("1.9.9") < *Version::parse ("1.10.0"));
            expect (*Version::parse ("2.0") == Version { 2, 0, 0 });
            expect (*Version::parse ("1.2.3-beta.1") == Version { 1, 2, 3 });
            expect (! Version::parse ("").has_value());
            expect (! Version::parse ("one.two").has_value());
            expect (! Version::parse ("1.2.3.4").has_value());
            expect (Version::parse (HARMONY_VERSION_STRING).has_value());
        }

        beginTest ("Release parsing");
        {
            auto r = parseRelease (release ("v9.1.0", true), platformInstallerSuffix());
            expect (r.has_value());
            expect (r->version == Version { 9, 1, 0 });
            expect (r->installerName.endsWith (platformInstallerSuffix()));
            expect (! parseRelease (release ("v9.1.0", false), platformInstallerSuffix()).has_value(), "checksums are mandatory");
            expect (! parseRelease (release ("v9.1.0", true, true), platformInstallerSuffix()).has_value(), "drafts are ignored");
            expect (! parseRelease (release ("latest", true), platformInstallerSuffix()).has_value());
            expect (! parseRelease (juce::var(), platformInstallerSuffix()).has_value());
        }

        beginTest ("Website manifest parsing");
        {
            const juce::String hash = juce::String::repeatedString ("ab", 32);
            const auto file = "YannosayHarmony-2.1.0" + platformInstallerSuffix();
            auto manifest = [&] (const juce::String& version, const juce::String& url, const juce::String& sha, const juce::String& fileName)
            {
                auto* o = new juce::DynamicObject();
                o->setProperty ("version", version);
                o->setProperty ("installer", url);
                o->setProperty ("sha256", sha);
                o->setProperty ("file", fileName);
                return juce::var (o);
            };

            const auto good = parseManifest (manifest ("2.1.0", "https://harmony.yannosay.com/download/" + file, hash, file), platformInstallerSuffix());
            expect (good.has_value());
            expect (good->version == Version { 2, 1, 0 });
            expect (good->sha256 == hash);
            expect (! parseManifest (manifest ("2.1.0", "http://harmony.yannosay.com/download/" + file, hash, file), platformInstallerSuffix()).has_value(), "https only");
            expect (! parseManifest (manifest ("2.1.0", "https://harmony.yannosay.com/download/" + file, "1234", file), platformInstallerSuffix()).has_value(), "hash required");
            expect (! parseManifest (manifest ("2.1.0", "https://evil.example/other.exe", hash, file), platformInstallerSuffix()).has_value(), "url must match file");
            expect (! parseManifest (manifest ("x", "https://harmony.yannosay.com/download/" + file, hash, file), platformInstallerSuffix()).has_value());
            expect (! parseManifest (juce::var ("text"), platformInstallerSuffix()).has_value());
        }

        beginTest ("Checksum lookup and verification");
        {
            const juce::String hash = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
            const auto sums = "0000000000000000000000000000000000000000000000000000000000000000  other.exe\n" + hash.toUpperCase() + " *setup.exe\n";
            expect (findChecksum (sums, "setup.exe") == hash);
            expect (findChecksum (sums, "missing.exe").isEmpty());

            juce::TemporaryFile file;
            file.getFile().replaceWithText ("abc");
            expect (verifySha256 (file.getFile(), hash));
            expect (! verifySha256 (file.getFile(), juce::String::repeatedString ("0", 64)));
            expect (! verifySha256 (file.getFile(), "short"));
        }
    }
};

static UpdaterTests updaterTests;
}

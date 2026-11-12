#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <shellapi.h>
#endif

#include "ui/SystemClipboard.h"
#include "model/MidiIO.h"

namespace harmony::ui::clipboard
{
#if defined (_WIN32)
namespace
{
    struct ScopedClipboard
    {
        ScopedClipboard() : open (OpenClipboard (nullptr) != 0) {}
        ~ScopedClipboard() { if (open) CloseClipboard(); }
        const bool open;
    };

    std::optional<juce::MidiFile> midiFromDrop()
    {
        auto handle = (HDROP) GetClipboardData (CF_HDROP);
        if (handle == nullptr)
            return std::nullopt;

        const UINT count = DragQueryFileW (handle, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < count; ++i)
        {
            wchar_t path[MAX_PATH * 4] {};
            if (DragQueryFileW (handle, i, path, (UINT) std::size (path)) == 0)
                continue;

            const juce::File file { juce::String (path) };
            if (midi::isMidiFile (file))
                if (auto m = midi::readFile (file))
                    return m;
        }
        return std::nullopt;
    }

    std::optional<juce::MidiFile> midiFromRawFormats()
    {
        for (UINT format = EnumClipboardFormats (0); format != 0; format = EnumClipboardFormats (format))
        {
            if (format == CF_HDROP || format == CF_BITMAP || format == CF_DIB || format == CF_DIBV5 || format == CF_ENHMETAFILE)
                continue;

            auto handle = GetClipboardData (format);
            if (handle == nullptr)
                continue;

            const auto size = GlobalSize (handle);
            if (size < 22 || size > 64 * 1024 * 1024)
                continue;

            const auto* bytes = static_cast<const char*> (GlobalLock (handle));
            if (bytes == nullptr)
                continue;

            std::optional<juce::MidiFile> result;
            for (size_t offset = 0; offset + 4 <= juce::jmin ((size_t) size, (size_t) 64); ++offset)
                if (std::memcmp (bytes + offset, "MThd", 4) == 0)
                {
                    result = midi::readData (bytes + offset, size - offset);
                    break;
                }

            GlobalUnlock (handle);
            if (result.has_value())
                return result;
        }
        return std::nullopt;
    }
}

juce::uint32 sequenceNumber()
{
    return (juce::uint32) GetClipboardSequenceNumber();
}

std::optional<juce::MidiFile> readMidi()
{
    const ScopedClipboard scoped;
    if (! scoped.open)
        return std::nullopt;

    if (auto fromFiles = midiFromDrop())
        return fromFiles;
    return midiFromRawFormats();
}
#else
juce::uint32 sequenceNumber() { return 0; }
std::optional<juce::MidiFile> readMidi() { return std::nullopt; }
#endif
}

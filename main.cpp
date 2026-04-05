#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>

#include <iostream>
#include <string>
#include <vector>
#include <sstream>

using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;

// converts a wide string (utf-16) to a regular utf-8 string
static std::string WideToUtf8(std::wstring_view ws)
{
    if (ws.empty()) return {};
    // first call gets the required buffer size
    int sz = WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string s(sz, '\0');
    // second call fills the buffer
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), s.data(), sz, nullptr, nullptr);
    return s;
}

// escapes special characters so the string is safe to put inside json quotes
static std::string EscapeJson(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"')       out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out += c;
    }
    return out;
}

// lookup table for base64 encoding
static const char kBase64Chars[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// encodes a block of bytes as a base64 string
static std::string ToBase64(const std::vector<uint8_t>& data)
{
    std::string out;
    int i = 0;
    uint8_t a3[3], a4[4];
    size_t len = data.size();
    size_t idx = 0;

    // process three bytes at a time and turn them into four base64 characters
    while (len--) {
        a3[i++] = data[idx++];
        if (i == 3) {
            a4[0] = (a3[0] & 0xfc) >> 2;
            a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
            a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
            a4[3] = a3[2] & 0x3f;
            for (i = 0; i < 4; i++) out += kBase64Chars[a4[i]];
            i = 0;
        }
    }

    // handle any leftover bytes that didn't fill a full group of three
    if (i) {
        // pad the group with zeros so the bit math still works
        for (int j = i; j < 3; j++) a3[j] = 0;
        a4[0] = (a3[0] & 0xfc) >> 2;
        a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
        a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
        for (int j = 0; j < i + 1; j++) out += kBase64Chars[a4[j]];
        // pad with '=' to make the output length a multiple of four
        while (i++ < 3) out += '=';
    }

    return out;
}

// turns a winrt playback status enum into a plain string for the json output
static const char* PlaybackStateStr(GlobalSystemMediaTransportControlsSessionPlaybackStatus s)
{
    switch (s) {
    case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing:  return "Playing";
    case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused:   return "Paused";
    case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Stopped:  return "Stopped";
    case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Changing: return "Changing";
    case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Opened:   return "Opened";
    case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Closed:   return "Closed";
    default:                                                                 return "Unknown";
    }
}

// reads the current media session from windows and prints it as a json object
IAsyncAction PrintMediaJson()
{
    // ask windows for the global media session manager
    auto manager = co_await GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
    if (!manager) {
        std::cout << "{}" << std::endl;
        co_return;
    }

    // get whichever app is currently in control of media (spotify, apple music, etc)
    auto session = manager.GetCurrentSession();
    if (!session) {
        std::cout << "{}" << std::endl;
        co_return;
    }

    // fetch the three info blocks — properties can be null if the app doesn't expose them
    auto props = co_await session.TryGetMediaPropertiesAsync();
    auto playback = session.GetPlaybackInfo();
    auto timeline = session.GetTimelineProperties();

    // windows stores time as 100-nanosecond ticks, so divide by 10 million to get seconds
    auto toSec = [](TimeSpan ts) {
        return (double)ts.count() / 10'000'000.0;
        };

    // read each field, falling back to an empty value if props isn't available
    std::string title = props ? EscapeJson(WideToUtf8(props.Title())) : "";
    std::string artist = props ? EscapeJson(WideToUtf8(props.Artist())) : "";
    std::string album = props ? EscapeJson(WideToUtf8(props.AlbumTitle())) : "";
    int         track = props ? props.TrackNumber() : 0;
    std::string status = playback ? PlaybackStateStr(playback.PlaybackStatus()) : "Unknown";
    double      position = timeline ? toSec(timeline.Position()) : 0.0;
    double      duration = timeline ? toSec(timeline.EndTime()) : 0.0;

    // read the thumbnail if one exists and encode it as base64 for the json
    std::string thumbnailBase64 = "";
    if (props && props.Thumbnail())
    {
        try {
            // open the thumbnail stream and read all its bytes into a buffer
            auto stream = co_await props.Thumbnail().OpenReadAsync();
            auto size = stream.Size();
            Buffer buf(static_cast<uint32_t>(size));
            co_await stream.ReadAsync(buf, static_cast<uint32_t>(size), InputStreamOptions::None);

            // copy from the winrt buffer into a plain byte vector
            auto dataReader = DataReader::FromBuffer(buf);
            std::vector<uint8_t> bytes(buf.Length());
            dataReader.ReadBytes(bytes);

            thumbnailBase64 = ToBase64(bytes);
        }
        catch (...) {}
    }

    // build and print the json — one object with all the fields
    std::ostringstream ss;
    ss << "{"
        << "\"Title\":\"" << title << "\","
        << "\"Artist\":\"" << artist << "\","
        << "\"Album\":\"" << album << "\","
        << "\"Status\":\"" << status << "\","
        << "\"ElapsedTime\":" << position << ","
        << "\"EndTime\":" << duration << ","
        << "\"ThumbnailBase64\":\"" << thumbnailBase64 << "\""
        << "}";

    std::cout << ss.str() << std::endl;
}

int main(int argc, char* argv[])
{
    // initialize the winrt runtime before using any winrt apis
    winrt::init_apartment();

    // only do anything if the -all flag is passed
    if (argc > 1 && std::string(argv[1]) == "-all")
        PrintMediaJson().get();

    return 0;
}
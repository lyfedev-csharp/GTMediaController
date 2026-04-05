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

static std::string WideToUtf8(std::wstring_view ws)
{
    if (ws.empty()) return {};
    int sz = WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string s(sz, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), s.data(), sz, nullptr, nullptr);
    return s;
}

static std::string EscapeJson(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out += c;
    }
    return out;
}

static const char kBase64Chars[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string ToBase64(const std::vector<uint8_t>& data)
{
    std::string out;
    int i = 0;
    uint8_t a3[3], a4[4];
    size_t len = data.size();
    size_t idx = 0;

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

    if (i) {
        for (int j = i; j < 3; j++) a3[j] = 0;
        a4[0] = (a3[0] & 0xfc) >> 2;
        a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
        a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
        for (int j = 0; j < i + 1; j++) out += kBase64Chars[a4[j]];
        while (i++ < 3) out += '=';
    }

    return out;
}

static const char* PlaybackStateStr(GlobalSystemMediaTransportControlsSessionPlaybackStatus s)
{
    switch (s) {
    case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing:  return "Playing";
    case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused:   return "Paused";
    case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Stopped:  return "Stopped";
    case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Changing: return "Changing";
    case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Opened:   return "Opened";
    case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Closed:   return "Closed";
    default:                                                                  return "Unknown";
    }
}

IAsyncAction PrintMediaJson()
{
    auto manager = co_await GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
    if (!manager) {
        std::cout << "{}" << std::endl;
        co_return;
    }

    auto session = manager.GetCurrentSession();
    if (!session) {
        std::cout << "{}" << std::endl;
        co_return;
    }

    auto props = co_await session.TryGetMediaPropertiesAsync();
    auto playback = session.GetPlaybackInfo();
    auto timeline = session.GetTimelineProperties();

    auto toSec = [](TimeSpan ts) {
        return (double)ts.count() / 10'000'000.0;
        };

    std::string title = props ? EscapeJson(WideToUtf8(props.Title())) : "";
    std::string artist = props ? EscapeJson(WideToUtf8(props.Artist())) : "";
    std::string album = props ? EscapeJson(WideToUtf8(props.AlbumTitle())) : "";
    int         track = props ? props.TrackNumber() : 0;
    std::string status = playback ? PlaybackStateStr(playback.PlaybackStatus()) : "Unknown";
    double      position = timeline ? toSec(timeline.Position()) : 0.0;
    double      duration = timeline ? toSec(timeline.EndTime()) : 0.0;

    std::string thumbnailBase64 = "";
    if (props && props.Thumbnail())
    {
        try {
            auto stream = co_await props.Thumbnail().OpenReadAsync();
            auto size = stream.Size();
            Buffer buf(static_cast<uint32_t>(size));
            co_await stream.ReadAsync(buf, static_cast<uint32_t>(size), InputStreamOptions::None);
            auto dataReader = DataReader::FromBuffer(buf);
            std::vector<uint8_t> bytes(buf.Length());
            dataReader.ReadBytes(bytes);
            thumbnailBase64 = ToBase64(bytes);
        }
        catch (...) {}
    }

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
    winrt::init_apartment();

    if (argc > 1 && std::string(argv[1]) == "-all")
        PrintMediaJson().get();

    return 0;
}
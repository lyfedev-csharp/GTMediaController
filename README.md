# GTMediaController

A lightweight C++ command-line utility that queries the Windows system media session and outputs the currently playing media information as JSON.

Used as a native bridge for [ChqserMedia](https://github.com/lyfedev-csharp/ChqserMedia) to retrieve real-time playback data from any media source on Windows.

---

## 📤 Output

Running with `-all` outputs a single JSON object to stdout:
```json
{
  "Title": "Cicada",
  "Artist": "Good Kid",
  "Album": "Nomu",
  "Status": "Playing",
  "ElapsedTime": 34.5,
  "EndTime": 166.0,
  "ThumbnailBase64": "..."
}
```

### Fields

| Field | Type | Description |
|---|---|---|
| `Title` | string | Track title |
| `Artist` | string | Artist name |
| `Album` | string | Album title |
| `Status` | string | `Playing`, `Paused`, `Stopped`, `Changing`, `Opened`, `Closed`, or `Unknown` |
| `ElapsedTime` | double | Current playback position in seconds |
| `EndTime` | double | Total track duration in seconds |
| `ThumbnailBase64` | string | Album art encoded as Base64 |

---

## 🚀 Usage
```bash
GTMediaController.exe -all
```

Running without `-all` does nothing and exits cleanly.

---

## 🛠️ Building

**Requirements:**
- Windows 10 or later
- Visual Studio 2019+ with C++/WinRT support
- Windows SDK 10.0.17763.0 or later

**Steps:**
1. Clone the repository
2. Open in Visual Studio
3. Build in Release x64
4. The output `GTMediaController.exe` can be used standalone

---

## ⚙️ How It Works

1. Requests the current `GlobalSystemMediaTransportControlsSessionManager` from Windows
2. Reads the active media session's properties, playback state, and timeline
3. If a thumbnail is available, reads it as a stream and encodes it to Base64
4. Outputs everything as a single JSON string to stdout and exits

Because it reads directly from the Windows media session, it works with any app that registers with the system media transport controls — Spotify, YouTube (in browsers), Windows Media Player, and more.

---

## 📄 License

MIT License. Free to use, modify, and distribute.

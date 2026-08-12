# floating clouds - foobar2000 plugus  
**Language**: **ENGLISH** | [简体中文](README_CN.md)  
***
Foobar2000 SDK Version: SDK-2025-03-07  
Windows Template Library: WTL10_01_Release    
***

A foobar2000 component that displays a floating UI overlay on your desktop or over fullscreen games, showing now-playing information with album art, track title, artist, and playback controls.

## Components

This repository contains three independent foobar2000 components:

| Component | DLL | What it does |
| --- | --- | --- |
| **Floating Clouds** | `foo_floating_clouds.dll` | This document's main subject — the floating now-playing overlay (see below) |
| **Playlist Organizer** | `foo_playlist_organizer.dll` | Organizes an active playlist by album artist into A-Z-sorted, locked per-artist playlists — [README](floating_clouds_organizing_playlists/README.md) |
| **Apple Music Tags** | `foo_floating_clouds_tags.dll` | Fetches per-region Apple Music tags and writes them to the selected tracks — [README](floating_clouds_tags/README.md) |

## Features

- **Floating overlay** — independent topmost window above the desktop or fullscreen games
- **6 styles** — Minimal, Full, Album Focus, Progress Ring, Visualizer, Lyrics Line
- **Material 3 (MD3) design** — dark `surface-container` card with rounded corners and a soft elevation shadow; truly transparent corners (per-pixel alpha)
- **Per-pixel alpha rendering** — UpdateLayeredWindow present path (driver-agnostic, avoids AMD DirectComposition flicker) with a uniform-alpha fallback
- **Smooth 60fps animation** — time-based (frame-rate independent) easing for progress, fades, button state layers, and show/hide
- **Customizable global hotkeys** — toggle drag mode, show/hide, cycle styles (works in fullscreen games)
- **Click-through** — buttons are clickable, everything else is transparent to input; drag mode moves the window
- **System tray** — right-click menu for style switching and visibility
- **Playlist picker** — ☰ button opens a two-level panel (playlists → tracks)
- **Lyrics** — embedded LRC/plain-text lyrics, synced to playback
- **Real-time visualizer** — smoothed FFT spectrum bars
- **Preferences page** — hotkeys, opacity, default style, auto-hide, UI language, debug logging

## Quick Start

1. Download the latest release from [Releases](https://github.com/Coconutat/floating-clouds---foobar2000-plugus/releases)
2. Install `foo_floating_clouds.fb2k-component` (or copy `foo_floating_clouds.dll`) into your foobar2000 `components` directory
3. Restart foobar2000 — the floating window appears automatically
4. Default hotkeys: `Ctrl+Alt+D` toggles drag mode, `Ctrl+Alt+F` hide/show, `Ctrl+Alt+S` cycle style

> All hotkeys can be customized in `File > Preferences > Components > Floating Clouds`: click a
> hotkey box, then press the new key combination (must include Ctrl/Alt/Shift/Win).

## Screenshots

<table>
  <tr>
    <td align="center"><img src="imgs/style-full.png" width="220" alt="Full"/><br/><b>Full</b></td>
    <td align="center"><img src="imgs/style-minimal.png" width="220" alt="Minimal"/><br/><b>Minimal</b></td>
    <td align="center"><img src="imgs/style-album-focus.png" width="220" alt="Album Focus"/><br/><b>Album Focus</b></td>
  </tr>
  <tr>
    <td align="center"><img src="imgs/style-progress-ring.png" width="220" alt="Progress Ring"/><br/><b>Progress Ring</b></td>
    <td align="center"><img src="imgs/style-visualizer.png" width="220" alt="Visualizer"/><br/><b>Visualizer</b></td>
    <td align="center"><img src="imgs/style-lyrics-line.png" width="220" alt="Lyrics Line"/><br/><b>Lyrics Line</b></td>
  </tr>
</table>

## Styles

| Style | Description |
| --- | --- |
| Minimal | Single line: play icon + title · artist + a thin bottom progress bar — low obstruction, ideal for fullscreen games |
| Full | Small album art + title/artist + progress bar + control buttons |
| Album Focus | Large album art as the hero visual, info and controls below |
| Progress Ring | Thumbnail surrounded by a circular progress ring |
| Visualizer | Real-time FFT spectrum bars + track info, game-HUD style |
| Lyrics Line | Current synced lyric line, visible at a glance during games |

## Requirements

- foobar2000 v2.0 or later (Windows 10 64-bit)

## Build

Requires Visual Studio 2019+ with C++17 support and the foobar2000 SDK.

The foobar2000 SDK is **not bundled** with this repository. Download the matching SDK version (2025-03-07) from <https://www.foobar2000.org/SDK> and extract it into an `SDK/` folder at the repository root — the project files reference `..\SDK\...` paths. Use it under its own license (`SDK/sdk-license.txt`).

```
git clone <repo>
powershell -ExecutionPolicy Bypass -File build.ps1            # Release / x64 -> foo_floating_clouds.dll
powershell -ExecutionPolicy Bypass -File build.ps1 -Package   # also package dist\foo_floating_clouds.fb2k-component
```

`build.ps1` also supports `-Configuration Debug`, `-Platform Win32`, `-Clean`, and
`-Deploy -Foobar2000Dir "<foobar2000 root>"`. Alternatively, open
`foo_floating_clouds/foo_floating_clouds.vcxproj` in Visual Studio and build.

## License

See [LICENSE](LICENSE) for details.
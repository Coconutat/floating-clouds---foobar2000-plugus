# floating clouds - foobar2000 plugus  
**Language**: **ENGLISH** | [简体中文](README_CN.md)  
***
Foobar2000 SDK Version: SDK-2025-03-07  
Windows Template Library: WTL10_01_Release    
***

A foobar2000 component that displays a floating UI overlay on your desktop or over fullscreen games, showing now-playing information with album art, track title, artist, and playback controls.

## Features

- **Floating overlay** — independent window that stays on top of desktop or games
- **3 core styles** — Mini, Mini Art, Full (with playback controls)
- **5 extended styles** — Minimal Line, Album Focus, Progress Ring, Visualizer, Lyrics Line
- **Customizable global hotkeys** — toggle drag mode, show/hide, cycle styles (works in fullscreen games)
- **Click-through** — buttons are clickable, rest is transparent to input
- **Dark / HUD-style** — semi-transparent dark panel, large rounded corners
- **Direct2D rendering** — hardware-accelerated, smooth animations
- **System tray** — right-click menu for style switching
- **Preferences page** — in foobar2000 Component Preferences

## Quick Start

1. Download the latest release from [Releases](https://github.com/Coconutat/floating-clouds---foobar2000-plugus/releases)
2. Copy `foo_floating_clouds.dll` to your foobar2000 `components` directory
3. Restart foobar2000 — the floating window appears automatically
4. Default hotkeys: `Ctrl+Alt+D` toggles drag mode, `Ctrl+Alt+F` hide/show, `Ctrl+Alt+S` cycle style

> All hotkeys can be customized in `File > Preferences > Components > Floating Clouds`: click a
> hotkey box, then press the new key combination (must include Ctrl/Alt/Shift/Win).

## Build

Requires Visual Studio 2019+ with C++17 support and the foobar2000 SDK (included).

```
git clone <repo>
# Open foo_floating_clouds/foo_floating_clouds.vcxproj in Visual Studio
# Build → foo_floating_clouds.dll
```

## License

See [LICENSE](LICENSE) for details.
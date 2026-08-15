#pragma once

#include <stdafx.h>

// ============================================================================
// Floating Clouds - Configuration definitions
// ============================================================================

// GUI IDs for preferences
namespace cfg_guids {
    // Window position
    static const GUID window_x = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x01 } };
    static const GUID window_y = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x02 } };

    // Appearance
    static const GUID opacity = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x03 } };
    static const GUID auto_hide = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x04 } };

    // Style
    static const GUID current_style = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x05 } };

    // Skin
    static const GUID current_skin = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x18 } };
    static const GUID color_mode = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x1B } };

    // Hotkeys - modifier keys (MOD_ALT | MOD_CONTROL etc)
    static const GUID hk_drag_mod = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x10 } };
    static const GUID hk_drag_vk = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x11 } };
    static const GUID hk_vis_mod = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x12 } };
    static const GUID hk_vis_vk = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x13 } };
    static const GUID hk_style_mod = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x14 } };
    static const GUID hk_style_vk = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x15 } };
    static const GUID hk_skin_mod = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x19 } };
    static const GUID hk_skin_vk = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x1A } };

    // Localization
    static const GUID language = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x16 } };

    // Diagnostics (Preferences > Appearance > Debug logging)
    static const GUID debug_logging = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x17 } };
}

// Style enum
enum class FloatingStyle : int32_t {
    Minimal = 0,      // was MinimalLine(3); absorbs Mini(0)
    Full = 1,         // was Full(2); absorbs MiniArt(1)
    AlbumFocus = 2,   // was 4
    ProgressRing = 3, // was 5
    Visualizer = 4,   // was 6
    LyricsLine = 5,   // was 7
    Count = 6
};

// Visual skin (material system), orthogonal to FloatingStyle (layout).
enum class FloatingSkin : int32_t {
    MD3 = 0,      // current Material 3 dark look (default)
    Apple = 1,    // Apple Design: dark liquid glass + system blue
    Count = 2
};

// Color mode: whether the skin uses its dark or light token set.
enum class FloatingColorMode : int32_t {
    Follow = 0,   // follow foobar2000 dark/light setting (default)
    Dark = 1,
    Light = 2,
    Count = 3
};

// Hotkey defaults (all customizable in Preferences > Components > Floating Clouds)
// Note: RegisterHotKey requires at least one modifier.
constexpr uint32_t DEFAULT_HK_DRAG_MOD = MOD_CONTROL | MOD_ALT;
constexpr uint32_t DEFAULT_HK_DRAG_VK = 'D';
constexpr uint32_t DEFAULT_HK_VIS_MOD = MOD_CONTROL | MOD_ALT;
constexpr uint32_t DEFAULT_HK_VIS_VK = 'F';
constexpr uint32_t DEFAULT_HK_STYLE_MOD = MOD_CONTROL | MOD_ALT;
constexpr uint32_t DEFAULT_HK_STYLE_VK = 'S';
constexpr uint32_t DEFAULT_HK_SKIN_MOD = MOD_CONTROL | MOD_ALT;
constexpr uint32_t DEFAULT_HK_SKIN_VK = 'T';

// Default values
constexpr int32_t DEFAULT_OPACITY = 220;
constexpr bool DEFAULT_AUTO_HIDE = true;
constexpr int32_t DEFAULT_STYLE = static_cast<int32_t>(FloatingStyle::Full);
constexpr int32_t DEFAULT_SKIN = static_cast<int32_t>(FloatingSkin::MD3);
constexpr int32_t DEFAULT_COLOR_MODE = static_cast<int32_t>(FloatingColorMode::Follow);
constexpr int32_t DEFAULT_LANGUAGE = 0; // 0 = English, 1 = Chinese

// Window styles (regular vs extended styles must not be mixed)
// NOTE: do NOT add WS_EX_TRANSPARENT to a layered window - per MSDN it makes ALL
// mouse events pass through (ignoring WM_NCHITTEST), so buttons can never be
// clicked. Click-through for non-interactive areas is done via WM_NCHITTEST
// returning HTTRANSPARENT instead.
constexpr DWORD FLOATING_WINDOW_STYLE = WS_POPUP;
// Base extended style; both present modes add WS_EX_LAYERED at runtime (ULW
// presents via UpdateLayeredWindow, the Hwnd fallback via LWA_ALPHA).
constexpr DWORD FLOATING_WINDOW_EX_STYLE = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;

// Message constants
constexpr UINT FC_WM_TRAY_NOTIFY = WM_APP + 1;
constexpr UINT FC_TRAY_ID = 0x9001;
constexpr UINT FC_HOVER_TIMER = 2; // polling timer for button hover feedback (anim timer uses 1)

// Window sizing
constexpr int32_t WINDOW_MIN_WIDTH = 200;
constexpr int32_t WINDOW_MIN_HEIGHT = 32;
constexpr int32_t WINDOW_PADDING = 12;
// Surface card corner radius (maps to md3::corner_large = 16) and the
// transparent margin around the card that the elevation shadow draws into.
constexpr int32_t WINDOW_CORNER_RADIUS = 16;
constexpr int32_t SHADOW_INSET = 8;

// Style sizing
constexpr int32_t STYLE_FULL_HEIGHT = 140;
// Fixed widths per style: the window never resizes with text length.
// Long titles scroll (marquee) inside their fixed width instead.
constexpr int32_t STYLE_MINIMAL_WIDTH = 360;
constexpr int32_t STYLE_MINIMAL_HEIGHT = 48;
constexpr int32_t STYLE_FULL_WIDTH = 400;
constexpr int32_t COVER_ART_SIZE_FULL = 64;
constexpr int32_t BUTTON_SIZE = 24;
constexpr int32_t BUTTON_SPACING = 8;
constexpr int32_t PROGRESS_BAR_HEIGHT = 4;

// Control buttons: prev, play/pause, next, mute, playlist picker.
constexpr int32_t BUTTON_COUNT = 5;

// --- Control-button row geometry (shared by hit-testing and style rendering) ---
inline int32_t button_row_count() { return BUTTON_COUNT; }
inline int32_t button_row_width()  { return BUTTON_COUNT * BUTTON_SIZE + (BUTTON_COUNT - 1) * BUTTON_SPACING; }
inline int32_t button_row_start_x(int32_t window_cx) { return (window_cx - button_row_width()) / 2; }
inline int32_t button_row_y(int32_t window_cy)       { return window_cy - WINDOW_PADDING - BUTTON_SIZE; }

// Time-based exponential approach (frame-rate independent). After ~3*tau
// seconds the value is ~95% toward target. tau in seconds.
inline float approach_dt(float current, float target, float tau, float dt) {
    const float k = 1.0f - expf(-dt / tau);
    return current + (target - current) * k;
}

// Which styles render a control-button row (tiny / special-purpose styles omit it).
inline bool style_has_buttons(FloatingStyle s) {
    switch (s) {
        case FloatingStyle::Full:
        case FloatingStyle::AlbumFocus:
        case FloatingStyle::ProgressRing:
        case FloatingStyle::Visualizer:
            return true;
        default:
            return false; // Minimal (single-line) and LyricsLine (lyrics overlay)
    }
}

// One-time migration from the old 8-style numbering (Mini=0..LyricsLine=7)
// to the new 6-style numbering. Applied when loading the persisted style.
inline int32_t migrate_style(int32_t old) {
    switch (old) {
        case 0: case 3: return 0; // Mini, MinimalLine -> Minimal
        case 1: case 2: return 1; // MiniArt, Full -> Full
        case 4: return 2;         // AlbumFocus
        case 5: return 3;         // ProgressRing
        case 6: return 4;         // Visualizer
        case 7: return 5;         // LyricsLine
        default: return 1;        // unknown -> Full (default)
    }
}
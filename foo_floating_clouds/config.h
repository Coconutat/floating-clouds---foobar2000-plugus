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

    // Hotkeys - modifier keys (MOD_ALT | MOD_CONTROL etc)
    static const GUID hk_drag_mod = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x10 } };
    static const GUID hk_drag_vk = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x11 } };
    static const GUID hk_vis_mod = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x12 } };
    static const GUID hk_vis_vk = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x13 } };
    static const GUID hk_style_mod = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x14 } };
    static const GUID hk_style_vk = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x15 } };

    // Localization
    static const GUID language = { 0x1a2b3c4d, 0x5e6f, 0x7890, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x16 } };
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

// Hotkey defaults (all customizable in Preferences > Components > Floating Clouds)
// Note: RegisterHotKey requires at least one modifier.
constexpr uint32_t DEFAULT_HK_DRAG_MOD = MOD_CONTROL | MOD_ALT;
constexpr uint32_t DEFAULT_HK_DRAG_VK = 'D';
constexpr uint32_t DEFAULT_HK_VIS_MOD = MOD_CONTROL | MOD_ALT;
constexpr uint32_t DEFAULT_HK_VIS_VK = 'F';
constexpr uint32_t DEFAULT_HK_STYLE_MOD = MOD_CONTROL | MOD_ALT;
constexpr uint32_t DEFAULT_HK_STYLE_VK = 'S';

// Default values
constexpr int32_t DEFAULT_OPACITY = 220;
constexpr bool DEFAULT_AUTO_HIDE = true;
constexpr int32_t DEFAULT_STYLE = static_cast<int32_t>(FloatingStyle::Full);
constexpr int32_t DEFAULT_LANGUAGE = 0; // 0 = English, 1 = Chinese

// Window styles (regular vs extended styles must not be mixed)
// NOTE: do NOT add WS_EX_TRANSPARENT to a layered window - per MSDN it makes ALL
// mouse events pass through (ignoring WM_NCHITTEST), so buttons can never be
// clicked. Click-through for non-interactive areas is done via WM_NCHITTEST
// returning HTTRANSPARENT instead.
constexpr DWORD FLOATING_WINDOW_STYLE = WS_POPUP;
constexpr DWORD FLOATING_WINDOW_EX_STYLE = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;

// Message constants
constexpr UINT FC_WM_TRAY_NOTIFY = WM_APP + 1;
constexpr UINT FC_TRAY_ID = 0x9001;
constexpr UINT FC_HOVER_TIMER = 2; // polling timer for button hover feedback (anim timer uses 1)

// Window sizing
constexpr int32_t WINDOW_MIN_WIDTH = 200;
constexpr int32_t WINDOW_MIN_HEIGHT = 32;
constexpr int32_t WINDOW_PADDING = 12;
constexpr int32_t WINDOW_CORNER_RADIUS = 12;

// Style sizing
constexpr int32_t STYLE_FULL_HEIGHT = 140;
// Fixed widths per style: the window never resizes with text length.
// Long titles scroll (marquee) inside their fixed width instead.
constexpr int32_t STYLE_MINIMAL_WIDTH = 360;
constexpr int32_t STYLE_FULL_WIDTH = 400;
constexpr int32_t COVER_ART_SIZE_FULL = 64;
constexpr int32_t BUTTON_SIZE = 24;
constexpr int32_t BUTTON_SPACING = 8;
constexpr int32_t PROGRESS_BAR_HEIGHT = 4;

// --- Control-button row geometry (shared by hit-testing and style rendering) ---
inline int32_t button_row_width()  { return 4 * BUTTON_SIZE + 3 * BUTTON_SPACING; }
inline int32_t button_row_start_x(int32_t window_cx) { return (window_cx - button_row_width()) / 2; }
inline int32_t button_row_y(int32_t window_cy)       { return window_cy - WINDOW_PADDING - BUTTON_SIZE; }
// Y of a progress bar that sits just above the button row.
inline int32_t progress_above_buttons_y(int32_t window_cy) {
    return window_cy - WINDOW_PADDING - BUTTON_SIZE - WINDOW_PADDING - PROGRESS_BAR_HEIGHT - 4;
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
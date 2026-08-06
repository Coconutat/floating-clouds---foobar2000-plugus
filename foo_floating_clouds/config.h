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
    Mini = 0,
    MiniArt = 1,
    Full = 2,
    // Extended styles (future)
    MinimalLine = 3,
    AlbumFocus = 4,
    ProgressRing = 5,
    Visualizer = 6,
    LyricsLine = 7,
    Count = 8
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
constexpr DWORD FLOATING_WINDOW_STYLE = WS_POPUP;
constexpr DWORD FLOATING_WINDOW_EX_STYLE = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;

// Message constants
constexpr UINT FC_WM_TRAY_NOTIFY = WM_APP + 1;
constexpr UINT FC_TRAY_ID = 0x9001;

// Window sizing
constexpr int32_t WINDOW_MIN_WIDTH = 200;
constexpr int32_t WINDOW_MIN_HEIGHT = 32;
constexpr int32_t WINDOW_PADDING = 12;
constexpr int32_t WINDOW_CORNER_RADIUS = 12;

// Style sizing
constexpr int32_t STYLE_MINI_HEIGHT = 40;
constexpr int32_t STYLE_MINIART_HEIGHT = 80;
constexpr int32_t STYLE_FULL_HEIGHT = 140;
constexpr int32_t COVER_ART_SIZE = 56;
constexpr int32_t COVER_ART_SIZE_FULL = 64;
constexpr int32_t BUTTON_SIZE = 24;
constexpr int32_t PROGRESS_BAR_HEIGHT = 4;
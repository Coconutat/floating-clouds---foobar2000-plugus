#pragma once

#include "stdafx.h"
#include "config.h"

// ============================================================================
// HotkeyManager - Global hotkey registration using RegisterHotKey
// ============================================================================

class HotkeyManager
{
public:
    HotkeyManager();
    ~HotkeyManager();

    // Register all hotkeys with the given window
    bool register_all(HWND hwnd);
    
    // Unregister all hotkeys
    void unregister_all();
    
    // Handle a WM_HOTKEY message - returns the action ID
    enum Action {
        ActionNone = 0,
        ActionToggleDrag,
        ActionToggleVisibility,
        ActionCycleStyle,
        ActionCycleSkin,
    };
    
    Action handle_hotkey(WPARAM wParam);

    // Get current hotkey info
    struct HotkeyInfo {
        uint32_t modifiers;
        uint32_t vk;
    };
    
    HotkeyInfo get_drag_hotkey();
    HotkeyInfo get_visibility_hotkey();
    HotkeyInfo get_style_hotkey();
    HotkeyInfo get_skin_hotkey();

private:
    bool register_single(HWND hwnd, uint32_t id, uint32_t modifiers, uint32_t vk);
    void unregister_single(uint32_t id);

    // Hotkey IDs (must be unique across the app)
    enum HotkeyId {
        ID_DRAG = 0x8001,
        ID_VISIBILITY = 0x8002,
        ID_STYLE = 0x8003,
        ID_SKIN = 0x8004,
    };

    struct RegisteredHotkey {
        uint32_t id;
        bool active;
        uint32_t modifiers;
        uint32_t vk;
    };

    // Only the live registrations are kept. The hotkey VALUES are read from cfg
    // on demand (see read_hotkey) instead of being cached in cfg_int members:
    // cfg_var_modern::cfg_int caches its value per instance (an atomic plus a
    // once_flag), so a long-lived member keeps returning whatever it read first
    // and silently ignores changes written by the Preferences page.
    std::vector<RegisteredHotkey> m_registered;
    HWND m_hwnd = nullptr;

    // Reads a hotkey slot out of cfg through a fresh instance, the same way the
    // rest of this component reads cfg.
    static HotkeyInfo read_hotkey(int slot);

    // Slot <-> id helpers so registration and reporting share one table.
    static const wchar_t* slot_name(int slot);
    bool register_slot(HWND hwnd, uint32_t id, int slot);
};
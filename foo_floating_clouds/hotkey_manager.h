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

    // Update a specific hotkey
    bool update_hotkey(HWND hwnd, uint32_t id, uint32_t modifiers, uint32_t vk);

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
    };
    
    std::vector<RegisteredHotkey> m_registered;
    HWND m_hwnd = nullptr;
    
    // Current values (backed by cfg_var)
    cfg_var_modern::cfg_int m_drag_mod;
    cfg_var_modern::cfg_int m_drag_vk;
    cfg_var_modern::cfg_int m_vis_mod;
    cfg_var_modern::cfg_int m_vis_vk;
    cfg_var_modern::cfg_int m_style_mod;
    cfg_var_modern::cfg_int m_style_vk;
    cfg_var_modern::cfg_int m_skin_mod;
    cfg_var_modern::cfg_int m_skin_vk;
};
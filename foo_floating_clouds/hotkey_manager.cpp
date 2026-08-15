#include "stdafx.h"
#include "hotkey_manager.h"

// ============================================================================
// HotkeyManager implementation
// ============================================================================

HotkeyManager::HotkeyManager()
    : m_drag_mod(cfg_guids::hk_drag_mod, DEFAULT_HK_DRAG_MOD)
    , m_drag_vk(cfg_guids::hk_drag_vk, DEFAULT_HK_DRAG_VK)
    , m_vis_mod(cfg_guids::hk_vis_mod, DEFAULT_HK_VIS_MOD)
    , m_vis_vk(cfg_guids::hk_vis_vk, DEFAULT_HK_VIS_VK)
    , m_style_mod(cfg_guids::hk_style_mod, DEFAULT_HK_STYLE_MOD)
    , m_style_vk(cfg_guids::hk_style_vk, DEFAULT_HK_STYLE_VK)
    , m_skin_mod(cfg_guids::hk_skin_mod, DEFAULT_HK_SKIN_MOD)
    , m_skin_vk(cfg_guids::hk_skin_vk, DEFAULT_HK_SKIN_VK)
{
}

HotkeyManager::~HotkeyManager()
{
    unregister_all();
}

bool HotkeyManager::register_all(HWND hwnd)
{
    m_hwnd = hwnd;
    
    bool ok = true;
    ok &= register_single(hwnd, ID_DRAG, (uint32_t)m_drag_mod.get_value(), (uint32_t)m_drag_vk.get_value());
    ok &= register_single(hwnd, ID_VISIBILITY, (uint32_t)m_vis_mod.get_value(), (uint32_t)m_vis_vk.get_value());
    ok &= register_single(hwnd, ID_STYLE, (uint32_t)m_style_mod.get_value(), (uint32_t)m_style_vk.get_value());
    ok &= register_single(hwnd, ID_SKIN, (uint32_t)m_skin_mod.get_value(), (uint32_t)m_skin_vk.get_value());
    
    return ok;
}

void HotkeyManager::unregister_all()
{
    for (auto& hk : m_registered) {
        if (hk.active) {
            ::UnregisterHotKey(m_hwnd, hk.id);
        }
    }
    m_registered.clear();
    m_hwnd = nullptr;
}

HotkeyManager::Action HotkeyManager::handle_hotkey(WPARAM wParam)
{
    uint32_t id = (uint32_t)wParam;
    switch (id) {
        case ID_DRAG: return ActionToggleDrag;
        case ID_VISIBILITY: return ActionToggleVisibility;
        case ID_STYLE: return ActionCycleStyle;
        case ID_SKIN: return ActionCycleSkin;
        default: return ActionNone;
    }
}

bool HotkeyManager::register_single(HWND hwnd, uint32_t id, uint32_t modifiers, uint32_t vk)
{
    // Unregister if already registered
    unregister_single(id);
    
    BOOL ok = ::RegisterHotKey(hwnd, id, modifiers, vk);
    m_registered.push_back({id, ok == TRUE});
    
    return ok == TRUE;
}

void HotkeyManager::unregister_single(uint32_t id)
{
    for (auto it = m_registered.begin(); it != m_registered.end(); ++it) {
        if (it->id == id) {
            if (it->active && m_hwnd) {
                ::UnregisterHotKey(m_hwnd, id);
            }
            m_registered.erase(it);
            return;
        }
    }
}

bool HotkeyManager::update_hotkey(HWND hwnd, uint32_t id, uint32_t modifiers, uint32_t vk)
{
    return register_single(hwnd, id, modifiers, vk);
}

HotkeyManager::HotkeyInfo HotkeyManager::get_drag_hotkey()
{
    return { (uint32_t)m_drag_mod.get_value(), (uint32_t)m_drag_vk.get_value() };
}

HotkeyManager::HotkeyInfo HotkeyManager::get_visibility_hotkey()
{
    return { (uint32_t)m_vis_mod.get_value(), (uint32_t)m_vis_vk.get_value() };
}

HotkeyManager::HotkeyInfo HotkeyManager::get_style_hotkey()
{
    return { (uint32_t)m_style_mod.get_value(), (uint32_t)m_style_vk.get_value() };
}

HotkeyManager::HotkeyInfo HotkeyManager::get_skin_hotkey()
{
    return { (uint32_t)m_skin_mod.get_value(), (uint32_t)m_skin_vk.get_value() };
}
#include "stdafx.h"
#include "hotkey_manager.h"

// ============================================================================
// HotkeyManager implementation
// ============================================================================

namespace {

// Slot order matches PrefsState (0 drag, 1 show/hide, 2 style, 3 skin).
const GUID* const k_mod_guid[4] = {
    &cfg_guids::hk_drag_mod, &cfg_guids::hk_vis_mod,
    &cfg_guids::hk_style_mod, &cfg_guids::hk_skin_mod,
};
const GUID* const k_vk_guid[4] = {
    &cfg_guids::hk_drag_vk, &cfg_guids::hk_vis_vk,
    &cfg_guids::hk_style_vk, &cfg_guids::hk_skin_vk,
};
const uint32_t k_mod_default[4] = {
    DEFAULT_HK_DRAG_MOD, DEFAULT_HK_VIS_MOD, DEFAULT_HK_STYLE_MOD, DEFAULT_HK_SKIN_MOD,
};
const uint32_t k_vk_default[4] = {
    DEFAULT_HK_DRAG_VK, DEFAULT_HK_VIS_VK, DEFAULT_HK_STYLE_VK, DEFAULT_HK_SKIN_VK,
};
const wchar_t* const k_slot_name[4] = { L"drag mode", L"show/hide", L"cycle style", L"cycle skin" };

} // namespace

HotkeyManager::HotkeyManager() = default;

HotkeyManager::~HotkeyManager()
{
    unregister_all();
}

HotkeyManager::HotkeyInfo HotkeyManager::read_hotkey(int slot)
{
    if (slot < 0 || slot > 3) return { 0, 0 };
    cfg_var_modern::cfg_int mod(*k_mod_guid[slot], k_mod_default[slot]);
    cfg_var_modern::cfg_int vk(*k_vk_guid[slot], k_vk_default[slot]);
    return { (uint32_t)mod.get_value(), (uint32_t)vk.get_value() };
}

const wchar_t* HotkeyManager::slot_name(int slot)
{
    return (slot >= 0 && slot <= 3) ? k_slot_name[slot] : L"?";
}

bool HotkeyManager::register_slot(HWND hwnd, uint32_t id, int slot)
{
    const HotkeyInfo hk = read_hotkey(slot);
    const bool ok = register_single(hwnd, id, hk.modifiers, hk.vk);
    if (ok) {
        // One line per slot, so "is the key I just set actually live?" is
        // answerable from the console instead of by trial and error.
        FB2K_console_formatter() << "Floating Clouds: hotkey '" << pfc::stringcvt::string_utf8_from_wide(slot_name(slot))
                                 << "' registered (mod=0x" << pfc::format_hex(hk.modifiers)
                                 << " vk=" << (int)hk.vk << ")";
    } else {
        // RegisterHotKey fails when another app (or foobar2000 itself) already
        // owns the combination; silence here is what made a re-bound hotkey look
        // like it had simply been ignored.
        FB2K_console_formatter() << "Floating Clouds: hotkey '" << pfc::stringcvt::string_utf8_from_wide(slot_name(slot))
                                 << "' could not be registered (mod=0x" << pfc::format_hex(hk.modifiers)
                                 << " vk=" << (int)hk.vk << ", error=" << (int)::GetLastError()
                                 << ") - another application may own it";
    }
    return ok;
}

bool HotkeyManager::register_all(HWND hwnd)
{
    m_hwnd = hwnd;

    bool ok = true;
    ok &= register_slot(hwnd, ID_DRAG, 0);
    ok &= register_slot(hwnd, ID_VISIBILITY, 1);
    ok &= register_slot(hwnd, ID_STYLE, 2);
    ok &= register_slot(hwnd, ID_SKIN, 3);
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
    
    // MOD_NOREPEAT: holding the combination must not toggle the action over and
    // over. It only affects registration, not the stored value.
    BOOL ok = ::RegisterHotKey(hwnd, id, modifiers | MOD_NOREPEAT, vk);
    m_registered.push_back({id, ok == TRUE, modifiers, vk});

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

HotkeyManager::HotkeyInfo HotkeyManager::get_drag_hotkey() { return read_hotkey(0); }
HotkeyManager::HotkeyInfo HotkeyManager::get_visibility_hotkey() { return read_hotkey(1); }
HotkeyManager::HotkeyInfo HotkeyManager::get_style_hotkey() { return read_hotkey(2); }
HotkeyManager::HotkeyInfo HotkeyManager::get_skin_hotkey() { return read_hotkey(3); }
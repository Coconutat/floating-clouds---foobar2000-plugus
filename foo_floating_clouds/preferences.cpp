#include "stdafx.h"
#include "resource.h"
#include "preferences.h"
#include "config.h"
#include "DarkMode.h"
#include "atl-misc.h"
#include "floating_window.h"
#include "localization.h"
#include <string>

// ============================================================================
// Floating Clouds Preferences page
// Uses the standard preferences_page_impl pattern from the SDK
// ============================================================================

// GUID for our preferences page
static constexpr GUID guid_preferences_page = { 0x2a3b4c5d, 0x6e7f, 0x8190, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x01 } };

// A hotkey edit control finished capturing: wParam = MAKEWPARAM(vk, modifiers), lParam = control ID
constexpr UINT FC_WM_HOTKEY_CAPTURED = WM_APP + 2;

// --- hotkey string helpers --------------------------------------------------

static std::wstring vk_name(UINT vk)
{
    if (vk >= 'A' && vk <= 'Z') return std::wstring(1, (wchar_t)vk);
    if (vk >= '0' && vk <= '9') return std::wstring(1, (wchar_t)vk);
    if (vk >= VK_F1 && vk <= VK_F24) return L"F" + std::to_wstring(vk - VK_F1 + 1);
    switch (vk) {
        case VK_SPACE: return L"Space";
        case VK_RETURN: return L"Enter";
        case VK_ESCAPE: return L"Esc";
        case VK_TAB: return L"Tab";
        case VK_BACK: return L"Backspace";
        case VK_DELETE: return L"Delete";
        case VK_INSERT: return L"Insert";
        case VK_HOME: return L"Home";
        case VK_END: return L"End";
        case VK_PRIOR: return L"Page Up";
        case VK_NEXT: return L"Page Down";
        case VK_LEFT: return L"\x2190";
        case VK_RIGHT: return L"\x2192";
        case VK_UP: return L"\x2191";
        case VK_DOWN: return L"\x2193";
        case VK_SCROLL: return L"Scroll Lock";
        case VK_PAUSE: return L"Pause";
        case VK_CAPITAL: return L"Caps Lock";
        case VK_NUMLOCK: return L"Num Lock";
        case VK_OEM_PLUS: return L"+";
        case VK_OEM_MINUS: return L"-";
        default: break;
    }
    // Fallback: let the system name the key (localized).
    UINT sc = MapVirtualKey(vk, MAPVK_VK_TO_VSC) << 16;
    if (sc) {
        wchar_t buf[64] = {};
        if (GetKeyNameTextW((LONG)sc, buf, 64) > 0) return std::wstring(buf);
    }
    return L"[" + std::to_wstring(vk) + L"]";
}

static std::wstring hotkey_to_string(uint32_t modifiers, uint32_t vk)
{
    std::wstring s;
    if (modifiers & MOD_CONTROL) s += L"Ctrl+";
    if (modifiers & MOD_ALT) s += L"Alt+";
    if (modifiers & MOD_SHIFT) s += L"Shift+";
    if (modifiers & MOD_WIN) s += L"Win+";
    s += vk_name(vk);
    return s;
}

// --- hotkey capture edit control --------------------------------------------
// Click the box, then press a new key combination (must include a modifier).

class HotkeyEdit : public CWindowImpl<HotkeyEdit, CEdit> {
public:
    HotkeyEdit() {}

    void SetHotkey(uint32_t modifiers, uint32_t vk)
    {
        m_mod = modifiers; m_vk = vk; m_armed = false;
        SetWindowText(hotkey_to_string(modifiers, vk).c_str());
    }

    BEGIN_MSG_MAP(HotkeyEdit)
        MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
        MESSAGE_HANDLER(WM_KILLFOCUS, OnKillFocus)
        MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
    END_MSG_MAP()

private:
    LRESULT OnSetFocus(UINT, WPARAM, LPARAM, BOOL& bHandled)
    {
        m_armed = true;
        SetWindowText(tr(L"Press new key combo...", L"按新的组合键..."));
        bHandled = FALSE;
        return 0;
    }

    LRESULT OnKillFocus(UINT, WPARAM, LPARAM, BOOL& bHandled)
    {
        if (m_armed) {
            m_armed = false;
            SetWindowText(hotkey_to_string(m_mod, m_vk).c_str());
        }
        bHandled = FALSE;
        return 0;
    }

    LRESULT OnKeyDown(UINT, WPARAM wParam, LPARAM, BOOL& bHandled)
    {
        bHandled = FALSE;
        if (!m_armed) return 0;

        UINT vk = (UINT)wParam;
        // Ignore presses of the modifier keys themselves; wait for the real key.
        if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN) {
            bHandled = TRUE;
            return 0;
        }

        uint32_t mod = 0;
        if (GetKeyState(VK_CONTROL) & 0x8000) mod |= MOD_CONTROL;
        if (GetKeyState(VK_MENU) & 0x8000) mod |= MOD_ALT;
        if (GetKeyState(VK_SHIFT) & 0x8000) mod |= MOD_SHIFT;
        if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000)) mod |= MOD_WIN;

        if (mod == 0) {
            SetWindowText(tr(L"Needs Ctrl/Alt/Shift/Win", L"需含 Ctrl/Alt/Shift/Win"));
            bHandled = TRUE;
            return 0;
        }

        m_armed = false;
        m_mod = mod;
        m_vk = vk;
        SetWindowText(hotkey_to_string(mod, vk).c_str());
        ::SendMessage(GetParent(), FC_WM_HOTKEY_CAPTURED, MAKEWPARAM(vk, mod), (LPARAM)GetDlgCtrlID());
        bHandled = TRUE;
        return 0;
    }

    bool m_armed = false;
    uint32_t m_mod = 0;
    uint32_t m_vk = 0;
};

class CMyPreferences : public CDialogImpl<CMyPreferences>, public preferences_page_instance {
public:
    CMyPreferences(preferences_page_callback::ptr callback) : m_callback(callback) {}

    enum { IDD = IDD_PREFERENCES };

    t_uint32 get_state() override {
        t_uint32 state = preferences_state::resettable | preferences_state::dark_mode_supported;
        if (HasChanged()) state |= preferences_state::changed;
        return state;
    }

    void apply() override;
    void reset() override;

    BEGIN_MSG_MAP_EX(CMyPreferences)
        MSG_WM_INITDIALOG(OnInitDialog)
        MSG_WM_HSCROLL(OnHScroll)
        COMMAND_HANDLER_EX(IDC_AUTO_HIDE, BN_CLICKED, OnAutoHideClicked)
        COMMAND_HANDLER_EX(IDC_DEBUG_LOG, BN_CLICKED, OnDebugLogClicked)
        COMMAND_HANDLER_EX(IDC_DEFAULT_STYLE, CBN_SELCHANGE, OnStyleChanged)
        COMMAND_HANDLER_EX(IDC_SKIN, CBN_SELCHANGE, OnSkinChanged)
        COMMAND_HANDLER_EX(IDC_LANGUAGE, CBN_SELCHANGE, OnLanguageChanged)
        MESSAGE_HANDLER(FC_WM_HOTKEY_CAPTURED, OnHotkeyCaptured)
    END_MSG_MAP()

private:
    BOOL OnInitDialog(CWindow, LPARAM);
    void OnHScroll(int nCode, int nPos, CScrollBar pScrollBar);
    void OnAutoHideClicked(UINT, int, CWindow);
    void OnDebugLogClicked(UINT, int, CWindow);
    void OnStyleChanged(UINT, int, CWindow);
    void OnSkinChanged(UINT, int, CWindow);
    void OnLanguageChanged(UINT, int, CWindow);
    void ReloadLocalizedStrings();
    LRESULT OnHotkeyCaptured(UINT, WPARAM wParam, LPARAM lParam, BOOL&);
    bool HasChanged();
    void OnChanged();
    void UpdateOpacityLabel(int opacity);

    const preferences_page_callback::ptr m_callback;
    bool m_initialized = false;

    // Dark mode support
    fb2k::CDarkModeHooks m_dark;

    // Hotkey capture boxes (subclassed edits)
    HotkeyEdit m_hk_drag, m_hk_vis, m_hk_style, m_hk_skin;
};

BOOL CMyPreferences::OnInitDialog(CWindow, LPARAM)
{
    m_dark.AddDialogWithControls(*this);

    // Read current values from cfg_var
    cfg_var_modern::cfg_int cfg_opacity(cfg_guids::opacity, DEFAULT_OPACITY);
    cfg_var_modern::cfg_bool cfg_auto_hide(cfg_guids::auto_hide, DEFAULT_AUTO_HIDE);
    cfg_var_modern::cfg_int cfg_style(cfg_guids::current_style, DEFAULT_STYLE);

    // Opacity slider
    CTrackBarCtrl slider = GetDlgItem(IDC_OPACITY);
    slider.SetRange(50, 255, TRUE);
    slider.SetPos((int)cfg_opacity.get_value());
    UpdateOpacityLabel((int)cfg_opacity.get_value());

    // Auto-hide checkbox
    Button_SetCheck(GetDlgItem(IDC_AUTO_HIDE), cfg_auto_hide.get() ? BST_CHECKED : BST_UNCHECKED);

    // Debug logging checkbox
    cfg_var_modern::cfg_bool cfg_debug(cfg_guids::debug_logging, false);
    Button_SetCheck(GetDlgItem(IDC_DEBUG_LOG), cfg_debug.get() ? BST_CHECKED : BST_UNCHECKED);

    // Style combo (localized names)
    CComboBox style_combo = GetDlgItem(IDC_DEFAULT_STYLE);
    for (int i = 0; i < (int)FloatingStyle::Count; i++) {
        style_combo.AddString(tr_style(static_cast<FloatingStyle>(i)));
    }
    style_combo.SetCurSel((int)cfg_style.get_value());

    // Skin combo (localized names)
    cfg_var_modern::cfg_int cfg_skin(cfg_guids::current_skin, DEFAULT_SKIN);
    CComboBox skin_combo = GetDlgItem(IDC_SKIN);
    for (int i = 0; i < (int)FloatingSkin::Count; i++) {
        skin_combo.AddString(tr_skin(static_cast<FloatingSkin>(i)));
    }
    int skin_sel = (int)cfg_skin.get_value();
    if (skin_sel < 0 || skin_sel >= (int)FloatingSkin::Count) skin_sel = DEFAULT_SKIN;
    skin_combo.SetCurSel(skin_sel);

    // Language combo
    CComboBox lang_combo = GetDlgItem(IDC_LANGUAGE);
    lang_combo.AddString(L"English");
    lang_combo.AddString(L"\x4E2D\x6587"); // 中文
    cfg_var_modern::cfg_int cfg_lang(cfg_guids::language, DEFAULT_LANGUAGE);
    int lang_sel = (int)cfg_lang.get_value();
    if (lang_sel < 0 || lang_sel > 1) lang_sel = 0;
    lang_combo.SetCurSel(lang_sel);

    // Apply localized texts for all controls
    ReloadLocalizedStrings();

    // Hotkey capture boxes - click a box, then press a new key combination
    m_hk_drag.SubclassWindow(GetDlgItem(IDC_HOTKEY_DRAG));
    m_hk_vis.SubclassWindow(GetDlgItem(IDC_HOTKEY_VISIBILITY));
    m_hk_style.SubclassWindow(GetDlgItem(IDC_HOTKEY_STYLE));
    m_hk_skin.SubclassWindow(GetDlgItem(IDC_HOTKEY_SKIN));
    {
        cfg_var_modern::cfg_int dm(cfg_guids::hk_drag_mod, DEFAULT_HK_DRAG_MOD);
        cfg_var_modern::cfg_int dv(cfg_guids::hk_drag_vk, DEFAULT_HK_DRAG_VK);
        cfg_var_modern::cfg_int vm(cfg_guids::hk_vis_mod, DEFAULT_HK_VIS_MOD);
        cfg_var_modern::cfg_int vv(cfg_guids::hk_vis_vk, DEFAULT_HK_VIS_VK);
        cfg_var_modern::cfg_int sm(cfg_guids::hk_style_mod, DEFAULT_HK_STYLE_MOD);
        cfg_var_modern::cfg_int sv(cfg_guids::hk_style_vk, DEFAULT_HK_STYLE_VK);
        cfg_var_modern::cfg_int km(cfg_guids::hk_skin_mod, DEFAULT_HK_SKIN_MOD);
        cfg_var_modern::cfg_int kv(cfg_guids::hk_skin_vk, DEFAULT_HK_SKIN_VK);
        m_hk_drag.SetHotkey((uint32_t)dm.get_value(), (uint32_t)dv.get_value());
        m_hk_vis.SetHotkey((uint32_t)vm.get_value(), (uint32_t)vv.get_value());
        m_hk_style.SetHotkey((uint32_t)sm.get_value(), (uint32_t)sv.get_value());
        m_hk_skin.SetHotkey((uint32_t)km.get_value(), (uint32_t)kv.get_value());
    }

    m_initialized = true;
    return FALSE;
}

void CMyPreferences::OnDebugLogClicked(UINT, int, CWindow)
{
    cfg_var_modern::cfg_bool cfg_debug(cfg_guids::debug_logging, false);
    cfg_debug = (Button_GetCheck(GetDlgItem(IDC_DEBUG_LOG)) == BST_CHECKED);
    FB2K_console_formatter() << "Floating Clouds: debug logging " << (cfg_debug.get() ? "ON" : "OFF");
}

void CMyPreferences::OnHScroll(int nCode, int nPos, CScrollBar pScrollBar)
{
    if (!m_initialized) return;
    CTrackBarCtrl slider = GetDlgItem(IDC_OPACITY);
    int pos = slider.GetPos();
    UpdateOpacityLabel(pos);
    cfg_var_modern::cfg_int cfg_opacity(cfg_guids::opacity, DEFAULT_OPACITY);
    cfg_opacity = pos;
    FloatingCloudsWindow::apply_preferences(); // hot reload
}

void CMyPreferences::OnAutoHideClicked(UINT, int, CWindow)
{
    cfg_var_modern::cfg_bool cfg_auto_hide(cfg_guids::auto_hide, DEFAULT_AUTO_HIDE);
    cfg_auto_hide = Button_GetCheck(GetDlgItem(IDC_AUTO_HIDE)) == BST_CHECKED;
}

void CMyPreferences::OnStyleChanged(UINT, int, CWindow)
{
    CComboBox style_combo = GetDlgItem(IDC_DEFAULT_STYLE);
    int sel = style_combo.GetCurSel();
    if (sel < 0) return;
    cfg_var_modern::cfg_int cfg_style(cfg_guids::current_style, DEFAULT_STYLE);
    cfg_style = sel;
    FloatingCloudsWindow::apply_preferences(); // hot reload
}

void CMyPreferences::OnSkinChanged(UINT, int, CWindow)
{
    CComboBox skin_combo = GetDlgItem(IDC_SKIN);
    int sel = skin_combo.GetCurSel();
    if (sel < 0) return;
    cfg_var_modern::cfg_int cfg_skin(cfg_guids::current_skin, DEFAULT_SKIN);
    cfg_skin = sel;
    FloatingCloudsWindow::apply_preferences(); // hot reload
}

void CMyPreferences::OnLanguageChanged(UINT, int, CWindow)
{
    CComboBox lang_combo = GetDlgItem(IDC_LANGUAGE);
    int sel = lang_combo.GetCurSel();
    if (sel < 0) return;
    cfg_var_modern::cfg_int cfg_lang(cfg_guids::language, DEFAULT_LANGUAGE);
    cfg_lang = sel;
    ReloadLocalizedStrings(); // re-localize the whole page immediately
}

void CMyPreferences::ReloadLocalizedStrings()
{
    // Style combo items (rebuild, restore selection from config)
    CComboBox style_combo = GetDlgItem(IDC_DEFAULT_STYLE);
    style_combo.ResetContent();
    for (int i = 0; i < (int)FloatingStyle::Count; i++) {
        style_combo.AddString(tr_style(static_cast<FloatingStyle>(i)));
    }
    cfg_var_modern::cfg_int cfg_style(cfg_guids::current_style, DEFAULT_STYLE);
    style_combo.SetCurSel((int)cfg_style.get_value());

    // Skin combo items (rebuild, restore selection from config)
    CComboBox skin_combo = GetDlgItem(IDC_SKIN);
    skin_combo.ResetContent();
    for (int i = 0; i < (int)FloatingSkin::Count; i++) {
        skin_combo.AddString(tr_skin(static_cast<FloatingSkin>(i)));
    }
    cfg_var_modern::cfg_int cfg_skin(cfg_guids::current_skin, DEFAULT_SKIN);
    int skin_sel = (int)cfg_skin.get_value();
    if (skin_sel < 0 || skin_sel >= (int)FloatingSkin::Count) skin_sel = DEFAULT_SKIN;
    skin_combo.SetCurSel(skin_sel);

    SetDlgItemText(IDC_GRP_HOTKEYS, tr(L"Hotkeys", L"热键"));
    SetDlgItemText(IDC_LBL_DRAG, tr(L"Drag mode toggle:", L"拖拽模式切换："));
    SetDlgItemText(IDC_LBL_SHOWHIDE, tr(L"Show/Hide:", L"显示/隐藏："));
    SetDlgItemText(IDC_LBL_CYCLE, tr(L"Cycle style:", L"切换样式："));
    SetDlgItemText(IDC_LBL_CYCLE_SKIN, tr(L"Cycle skin:", L"切换皮肤："));
    SetDlgItemText(IDC_LBL_HOTKEY_HINT, tr(L"Click a box, then press the new key combo (needs Ctrl/Alt/Shift/Win)", L"点击输入框后按新组合键（须含 Ctrl/Alt/Shift/Win）"));
    SetDlgItemText(IDC_GRP_APPEARANCE, tr(L"Appearance", L"外观"));
    SetDlgItemText(IDC_LBL_OPACITY, tr(L"Opacity:", L"透明度："));
    SetDlgItemText(IDC_AUTO_HIDE, tr(L"Auto-hide when stopped", L"停止时自动隐藏"));
    SetDlgItemText(IDC_GRP_STYLE, tr(L"Style", L"样式"));
    SetDlgItemText(IDC_LBL_DEFAULT_STYLE, tr(L"Default style:", L"默认样式："));
    SetDlgItemText(IDC_GRP_SKIN, tr(L"Skin", L"皮肤"));
    SetDlgItemText(IDC_LBL_DEFAULT_SKIN, tr(L"Default skin:", L"默认皮肤："));
    SetDlgItemText(IDC_GRP_LANGUAGE, tr(L"Language", L"语言"));
    SetDlgItemText(IDC_LBL_LANGUAGE, tr(L"UI Language:", L"界面语言："));
}

LRESULT CMyPreferences::OnHotkeyCaptured(UINT, WPARAM wParam, LPARAM lParam, BOOL&)
{
    uint32_t mod = HIWORD(wParam);
    uint32_t vk = LOWORD(wParam);
    int ctrl_id = (int)lParam;

    const GUID* g_mod = nullptr;
    const GUID* g_vk = nullptr;
    uint32_t d_mod = 0, d_vk = 0;
    switch (ctrl_id) {
        case IDC_HOTKEY_DRAG:
            g_mod = &cfg_guids::hk_drag_mod; g_vk = &cfg_guids::hk_drag_vk;
            d_mod = DEFAULT_HK_DRAG_MOD; d_vk = DEFAULT_HK_DRAG_VK;
            break;
        case IDC_HOTKEY_VISIBILITY:
            g_mod = &cfg_guids::hk_vis_mod; g_vk = &cfg_guids::hk_vis_vk;
            d_mod = DEFAULT_HK_VIS_MOD; d_vk = DEFAULT_HK_VIS_VK;
            break;
        case IDC_HOTKEY_STYLE:
            g_mod = &cfg_guids::hk_style_mod; g_vk = &cfg_guids::hk_style_vk;
            d_mod = DEFAULT_HK_STYLE_MOD; d_vk = DEFAULT_HK_STYLE_VK;
            break;
        case IDC_HOTKEY_SKIN:
            g_mod = &cfg_guids::hk_skin_mod; g_vk = &cfg_guids::hk_skin_vk;
            d_mod = DEFAULT_HK_SKIN_MOD; d_vk = DEFAULT_HK_SKIN_VK;
            break;
        default:
            return 0;
    }

    // Persist and apply immediately.
    cfg_var_modern::cfg_int cm(*g_mod, d_mod);
    cfg_var_modern::cfg_int cv(*g_vk, d_vk);
    cm = (int64_t)mod;
    cv = (int64_t)vk;

    FB2K_console_formatter() << "Floating Clouds: hotkey " << ctrl_id << " set mod=0x" << pfc::format_hex(mod) << " vk=" << vk;
    FloatingCloudsWindow::reload_hotkeys();
    return 0;
}

void CMyPreferences::UpdateOpacityLabel(int opacity)
{
    pfc::string8 text;
    text << (int)(opacity / 255.0f * 100) << "%";
    SetDlgItemTextA(m_hWnd, IDC_OPACITY_LABEL, text);
}

bool CMyPreferences::HasChanged()
{
    cfg_var_modern::cfg_int cfg_opacity(cfg_guids::opacity, DEFAULT_OPACITY);
    cfg_var_modern::cfg_bool cfg_auto_hide(cfg_guids::auto_hide, DEFAULT_AUTO_HIDE);
    cfg_var_modern::cfg_int cfg_style(cfg_guids::current_style, DEFAULT_STYLE);
    cfg_var_modern::cfg_int cfg_skin(cfg_guids::current_skin, DEFAULT_SKIN);

    CTrackBarCtrl slider = GetDlgItem(IDC_OPACITY);
    int opacity = slider.GetPos();

    bool auto_hide = Button_GetCheck(GetDlgItem(IDC_AUTO_HIDE)) == BST_CHECKED;
    CComboBox style_combo = GetDlgItem(IDC_DEFAULT_STYLE);
    int style = style_combo.GetCurSel();
    CComboBox skin_combo = GetDlgItem(IDC_SKIN);
    int skin = skin_combo.GetCurSel();

    return opacity != (int)cfg_opacity.get_value() ||
           auto_hide != cfg_auto_hide.get() ||
           (style >= 0 && style != (int)cfg_style.get_value()) ||
           (skin >= 0 && skin != (int)cfg_skin.get_value());
}

void CMyPreferences::OnChanged()
{
    m_callback->on_state_changed();
}

void CMyPreferences::apply()
{
    CTrackBarCtrl slider = GetDlgItem(IDC_OPACITY);
    int opacity = slider.GetPos();
    cfg_var_modern::cfg_int cfg_opacity(cfg_guids::opacity, DEFAULT_OPACITY);
    cfg_opacity = opacity;

    bool auto_hide = Button_GetCheck(GetDlgItem(IDC_AUTO_HIDE)) == BST_CHECKED;
    cfg_var_modern::cfg_bool cfg_auto_hide(cfg_guids::auto_hide, DEFAULT_AUTO_HIDE);
    cfg_auto_hide = auto_hide;

    CComboBox style_combo = GetDlgItem(IDC_DEFAULT_STYLE);
    int style = style_combo.GetCurSel();
    if (style >= 0) {
        cfg_var_modern::cfg_int cfg_style(cfg_guids::current_style, DEFAULT_STYLE);
        cfg_style = style;
    }

    CComboBox skin_combo = GetDlgItem(IDC_SKIN);
    int skin = skin_combo.GetCurSel();
    if (skin >= 0) {
        cfg_var_modern::cfg_int cfg_skin(cfg_guids::current_skin, DEFAULT_SKIN);
        cfg_skin = skin;
    }

    FloatingCloudsWindow::apply_preferences();
    OnChanged();
}

void CMyPreferences::reset()
{
    CTrackBarCtrl slider = GetDlgItem(IDC_OPACITY);
    slider.SetPos(DEFAULT_OPACITY);
    UpdateOpacityLabel(DEFAULT_OPACITY);

    Button_SetCheck(GetDlgItem(IDC_AUTO_HIDE), DEFAULT_AUTO_HIDE ? BST_CHECKED : BST_UNCHECKED);

    CComboBox style_combo = GetDlgItem(IDC_DEFAULT_STYLE);
    style_combo.SetCurSel(DEFAULT_STYLE);

    CComboBox skin_combo = GetDlgItem(IDC_SKIN);
    skin_combo.SetCurSel(DEFAULT_SKIN);

    // Hotkeys -> defaults + re-register
    {
        cfg_var_modern::cfg_int dm(cfg_guids::hk_drag_mod, DEFAULT_HK_DRAG_MOD);
        cfg_var_modern::cfg_int dv(cfg_guids::hk_drag_vk, DEFAULT_HK_DRAG_VK);
        cfg_var_modern::cfg_int vm(cfg_guids::hk_vis_mod, DEFAULT_HK_VIS_MOD);
        cfg_var_modern::cfg_int vv(cfg_guids::hk_vis_vk, DEFAULT_HK_VIS_VK);
        cfg_var_modern::cfg_int sm(cfg_guids::hk_style_mod, DEFAULT_HK_STYLE_MOD);
        cfg_var_modern::cfg_int sv(cfg_guids::hk_style_vk, DEFAULT_HK_STYLE_VK);
        cfg_var_modern::cfg_int km(cfg_guids::hk_skin_mod, DEFAULT_HK_SKIN_MOD);
        cfg_var_modern::cfg_int kv(cfg_guids::hk_skin_vk, DEFAULT_HK_SKIN_VK);
        dm = DEFAULT_HK_DRAG_MOD; dv = DEFAULT_HK_DRAG_VK;
        vm = DEFAULT_HK_VIS_MOD; vv = DEFAULT_HK_VIS_VK;
        sm = DEFAULT_HK_STYLE_MOD; sv = DEFAULT_HK_STYLE_VK;
        km = DEFAULT_HK_SKIN_MOD; kv = DEFAULT_HK_SKIN_VK;
        m_hk_drag.SetHotkey(DEFAULT_HK_DRAG_MOD, DEFAULT_HK_DRAG_VK);
        m_hk_vis.SetHotkey(DEFAULT_HK_VIS_MOD, DEFAULT_HK_VIS_VK);
        m_hk_style.SetHotkey(DEFAULT_HK_STYLE_MOD, DEFAULT_HK_STYLE_VK);
        m_hk_skin.SetHotkey(DEFAULT_HK_SKIN_MOD, DEFAULT_HK_SKIN_VK);
        FloatingCloudsWindow::reload_hotkeys();
    }

    FloatingCloudsWindow::apply_preferences();
    OnChanged();
}

// preferences_page_impl<> is defined in atl-misc.h - it inherits from preferences_page_v3
// and provides instantiate() that creates our dialog
class preferences_page_myimpl : public preferences_page_impl<CMyPreferences> {
public:
    const char* get_name() { return "Floating Clouds"; }
    GUID get_guid() { return guid_preferences_page; }
    GUID get_parent_guid() { return preferences_page::guid_components; }
};

static preferences_page_factory_t<preferences_page_myimpl> g_preferences_page_factory;
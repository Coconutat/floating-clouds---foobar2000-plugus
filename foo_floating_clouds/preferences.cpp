#include "stdafx.h"
#include "resource.h"
#include "preferences.h"
#include "config.h"
#include "DarkMode.h"
#include "atl-misc.h"
#include "floating_window.h"
#include "localization.h"
#include <string>
#include <vector>

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

// --- installed font faces ---------------------------------------------------
// IDC_FONT_FAMILY item 0 is "(Follow foobar2000)" (cfg stores ""); every other
// item is a face reported by the system. lfFaceName is already the localized
// family name, so Chinese faces show up in Chinese without any extra work.

static int CALLBACK enum_font_proc(const LOGFONTW* lf, const TEXTMETRICW*, DWORD, LPARAM lp)
{
    if (lf->lfFaceName[0] == L'@') return 1; // vertical CJK alias of a normal face
    auto* faces = reinterpret_cast<std::vector<std::wstring>*>(lp);
    for (const auto& f : *faces) {
        if (::CompareStringOrdinal(f.c_str(), -1, lf->lfFaceName, -1, TRUE) == CSTR_EQUAL) return 1;
    }
    faces->push_back(lf->lfFaceName);
    return 1;
}

// ponytail: a screen DC is enough to enumerate names; no font object needed.
static std::vector<std::wstring> enum_font_faces()
{
    std::vector<std::wstring> faces;
    LOGFONTW lf = {};
    lf.lfCharSet = DEFAULT_CHARSET;
    HDC dc = ::GetDC(nullptr);
    if (dc) {
        ::EnumFontFamiliesExW(dc, &lf, enum_font_proc, reinterpret_cast<LPARAM>(&faces), 0);
        ::ReleaseDC(nullptr, dc);
    }
    // Enumeration order is per-charset groups, not alphabetical; sort for the dropdown.
    (std::sort)(faces.begin(), faces.end(), [](const std::wstring& a, const std::wstring& b) {
        return ::CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN;
    });
    return faces;
}

// Rebuild a localized-name combo, keeping the current selection.
template <typename NameFn>
static void refill_combo(CComboBox combo, int count, NameFn name)
{
    int sel = combo.GetCurSel();
    combo.ResetContent();
    for (int i = 0; i < count; i++) combo.AddString(name(i));
    combo.SetCurSel(sel);
}

// --- cfg snapshot -----------------------------------------------------------
// PrefsState lives in preferences.h; the key tables and writers stay here.
namespace {
const GUID* const k_hk_mod_guids[4] = { &cfg_guids::hk_drag_mod, &cfg_guids::hk_vis_mod, &cfg_guids::hk_style_mod, &cfg_guids::hk_skin_mod };
const GUID* const k_hk_vk_guids[4]  = { &cfg_guids::hk_drag_vk, &cfg_guids::hk_vis_vk, &cfg_guids::hk_style_vk, &cfg_guids::hk_skin_vk };
const uint32_t k_hk_mod_defaults[4] = { DEFAULT_HK_DRAG_MOD, DEFAULT_HK_VIS_MOD, DEFAULT_HK_STYLE_MOD, DEFAULT_HK_SKIN_MOD };
const uint32_t k_hk_vk_defaults[4]  = { DEFAULT_HK_DRAG_VK, DEFAULT_HK_VIS_VK, DEFAULT_HK_STYLE_VK, DEFAULT_HK_SKIN_VK };

void write_hotkeys(const PrefsState& s)
{
    for (int i = 0; i < 4; i++) {
        cfg_var_modern::cfg_int mod(*k_hk_mod_guids[i], k_hk_mod_defaults[i]);
        cfg_var_modern::cfg_int vk(*k_hk_vk_guids[i], k_hk_vk_defaults[i]);
        mod = (int64_t)s.hk_mod[i];
        vk = (int64_t)s.hk_vk[i];
    }
}
} // namespace

void PrefsState::load()
{
    cfg_var_modern::cfg_int cfg_opacity(cfg_guids::opacity, DEFAULT_OPACITY);
    cfg_var_modern::cfg_bool cfg_auto_hide(cfg_guids::auto_hide, DEFAULT_AUTO_HIDE);
    cfg_var_modern::cfg_bool cfg_debug(cfg_guids::debug_logging, false);
    cfg_var_modern::cfg_bool cfg_attached(cfg_guids::vis_art_attached_baseline, false);
    cfg_var_modern::cfg_int cfg_style(cfg_guids::current_style, DEFAULT_STYLE);
    cfg_var_modern::cfg_int cfg_skin(cfg_guids::current_skin, DEFAULT_SKIN);
    cfg_var_modern::cfg_int cfg_mode(cfg_guids::color_mode, DEFAULT_COLOR_MODE);
    cfg_var_modern::cfg_int cfg_lang(cfg_guids::language, DEFAULT_LANGUAGE);
    cfg_var_modern::cfg_string cfg_font(cfg_guids::font_family, "");

    opacity = (int)cfg_opacity.get_value();
    auto_hide = cfg_auto_hide.get();
    debug_log = cfg_debug.get();
    attached_baseline = cfg_attached.get();
    style = (int)cfg_style.get_value();
    skin = (int)cfg_skin.get_value();
    color_mode = (int)cfg_mode.get_value();
    language = (int)cfg_lang.get_value();
    const char* f = cfg_font.get();
    font = f ? f : "";

    for (int i = 0; i < 4; i++) {
        cfg_var_modern::cfg_int mod(*k_hk_mod_guids[i], k_hk_mod_defaults[i]);
        cfg_var_modern::cfg_int vk(*k_hk_vk_guids[i], k_hk_vk_defaults[i]);
        hk_mod[i] = (uint32_t)mod.get_value();
        hk_vk[i] = (uint32_t)vk.get_value();
    }
}

void PrefsState::apply() const
{
    { cfg_var_modern::cfg_int v(cfg_guids::opacity, DEFAULT_OPACITY); v = opacity; }
    { cfg_var_modern::cfg_bool v(cfg_guids::auto_hide, DEFAULT_AUTO_HIDE); v = auto_hide; }
    { cfg_var_modern::cfg_bool v(cfg_guids::debug_logging, false); v = debug_log; }
    { cfg_var_modern::cfg_bool v(cfg_guids::vis_art_attached_baseline, false); v = attached_baseline; }
    { cfg_var_modern::cfg_int v(cfg_guids::current_style, DEFAULT_STYLE); v = style; }
    { cfg_var_modern::cfg_int v(cfg_guids::current_skin, DEFAULT_SKIN); v = skin; }
    { cfg_var_modern::cfg_int v(cfg_guids::color_mode, DEFAULT_COLOR_MODE); v = color_mode; }
    { cfg_var_modern::cfg_int v(cfg_guids::language, DEFAULT_LANGUAGE); v = language; }
    { cfg_var_modern::cfg_string v(cfg_guids::font_family, ""); v = font.c_str(); }
    write_hotkeys(*this);
}

bool PrefsState::equals(const PrefsState& other) const
{
    // Controls are live-persisted, but the ones listed here stay tracked so the
    // page still reports "changed" and Apply keeps its meaning. Language, debug
    // logging and the hotkeys take effect instantly and never dirty the page.
    return opacity == other.opacity &&
           auto_hide == other.auto_hide &&
           attached_baseline == other.attached_baseline &&
           style == other.style &&
           skin == other.skin &&
           color_mode == other.color_mode &&
           font == other.font;
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

    uint32_t get_mod() const { return m_mod; }
    uint32_t get_vk() const { return m_vk; }

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
        COMMAND_HANDLER_EX(IDC_ATTACHED_BASELINE, BN_CLICKED, OnAttachedBaselineClicked)
        COMMAND_HANDLER_EX(IDC_DEFAULT_STYLE, CBN_SELCHANGE, OnStyleChanged)
        COMMAND_HANDLER_EX(IDC_SKIN, CBN_SELCHANGE, OnSkinChanged)
        COMMAND_HANDLER_EX(IDC_COLOR_MODE, CBN_SELCHANGE, OnColorModeChanged)
        COMMAND_HANDLER_EX(IDC_LANGUAGE, CBN_SELCHANGE, OnLanguageChanged)
        COMMAND_HANDLER_EX(IDC_FONT_FAMILY, CBN_SELCHANGE, OnFontChanged)
        MESSAGE_HANDLER(FC_WM_HOTKEY_CAPTURED, OnHotkeyCaptured)
    END_MSG_MAP()

private:
    BOOL OnInitDialog(CWindow, LPARAM);
    void OnHScroll(int nCode, int nPos, CScrollBar pScrollBar);
    void OnAutoHideClicked(UINT, int, CWindow);
    void OnDebugLogClicked(UINT, int, CWindow);
    void OnAttachedBaselineClicked(UINT, int, CWindow);
    void OnStyleChanged(UINT, int, CWindow);
    void OnSkinChanged(UINT, int, CWindow);
    void OnColorModeChanged(UINT, int, CWindow);
    void OnLanguageChanged(UINT, int, CWindow);
    void OnFontChanged(UINT, int, CWindow);
    void ReloadLocalizedStrings();
    LRESULT OnHotkeyCaptured(UINT, WPARAM wParam, LPARAM lParam, BOOL&);
    void SetControls(const PrefsState& s);
    PrefsState ReadUI();
    void Commit();
    // Colour mode is meaningless for Liquid Glass: show one disabled entry and
    // leave the stored preference alone.
    void RefreshColorModeCombo();
    bool HasChanged();
    void OnChanged();
    void UpdateOpacityLabel(int opacity);

    const preferences_page_callback::ptr m_callback;
    bool m_initialized = false;
    PrefsState m_loaded; // cfg snapshot at open / after the last apply

    // Dark mode support
    fb2k::CDarkModeHooks m_dark;

    // Hotkey capture boxes (subclassed edits)
    HotkeyEdit m_hk_drag, m_hk_vis, m_hk_style, m_hk_skin;
};

BOOL CMyPreferences::OnInitDialog(CWindow, LPARAM)
{
    m_dark.AddDialogWithControls(*this);

    m_loaded.load();

    // Hotkey capture boxes - click a box, then press a new key combination
    m_hk_drag.SubclassWindow(GetDlgItem(IDC_HOTKEY_DRAG));
    m_hk_vis.SubclassWindow(GetDlgItem(IDC_HOTKEY_VISIBILITY));
    m_hk_style.SubclassWindow(GetDlgItem(IDC_HOTKEY_STYLE));
    m_hk_skin.SubclassWindow(GetDlgItem(IDC_HOTKEY_SKIN));

    SetControls(m_loaded);
    ReloadLocalizedStrings();

    m_initialized = true;
    return FALSE;
}

void CMyPreferences::SetControls(const PrefsState& s)
{
    CTrackBarCtrl slider = GetDlgItem(IDC_OPACITY);
    slider.SetRange(50, 255, TRUE);
    slider.SetPos(s.opacity);
    UpdateOpacityLabel(s.opacity);

    Button_SetCheck(GetDlgItem(IDC_AUTO_HIDE), s.auto_hide ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(GetDlgItem(IDC_DEBUG_LOG), s.debug_log ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(GetDlgItem(IDC_ATTACHED_BASELINE), s.attached_baseline ? BST_CHECKED : BST_UNCHECKED);

    CComboBox style_combo = GetDlgItem(IDC_DEFAULT_STYLE);
    refill_combo(style_combo, (int)FloatingStyle::Count, [](int i) { return tr_style(static_cast<FloatingStyle>(i)); });
    int style = s.style;
    if (style < 0 || style >= (int)FloatingStyle::Count) style = DEFAULT_STYLE;
    style_combo.SetCurSel(style);

    CComboBox skin_combo = GetDlgItem(IDC_SKIN);
    refill_combo(skin_combo, (int)FloatingSkin::Count, [](int i) { return tr_skin(static_cast<FloatingSkin>(i)); });
    int skin = s.skin;
    if (skin < 0 || skin >= (int)FloatingSkin::Count) skin = DEFAULT_SKIN;
    skin_combo.SetCurSel(skin);

    CComboBox mode_combo = GetDlgItem(IDC_COLOR_MODE);
    RefreshColorModeCombo();
    int mode = s.color_mode;
    if (mode < 0 || mode >= (int)FloatingColorMode::Count) mode = DEFAULT_COLOR_MODE;
    mode_combo.SetCurSel(mode);

    // Font family: item 0 = follow foobar2000, then every installed face.
    CComboBox font_combo = GetDlgItem(IDC_FONT_FAMILY);
    font_combo.ResetContent();
    font_combo.AddString(tr(L"(Follow foobar2000)", L"(跟随 foobar2000)"));
    pfc::stringcvt::string_wide_from_utf8 wide(s.font.c_str());
    const wchar_t* want = wide.is_empty() ? L"" : wide.get_ptr();
    int font_sel = 0;
    for (const auto& face : enum_font_faces()) {
        int idx = font_combo.AddString(face.c_str());
        if (font_sel == 0 && *want && ::CompareStringOrdinal(face.c_str(), -1, want, -1, TRUE) == CSTR_EQUAL) font_sel = idx;
    }
    // Stored face no longer installed: keep it selectable instead of dropping it.
    if (font_sel == 0 && *want) {
        int idx = font_combo.AddString(want);
        if (idx >= 0) font_sel = idx;
    }
    font_combo.SetCurSel(font_sel >= 0 ? font_sel : 0);

    CComboBox lang_combo = GetDlgItem(IDC_LANGUAGE);
    lang_combo.ResetContent();
    lang_combo.AddString(L"English");
    lang_combo.AddString(L"\x4E2D\x6587"); // 中文
    lang_combo.SetCurSel(s.language == (int)PluginLanguage::Chinese ? 1 : 0);

    m_hk_drag.SetHotkey(s.hk_mod[0], s.hk_vk[0]);
    m_hk_vis.SetHotkey(s.hk_mod[1], s.hk_vk[1]);
    m_hk_style.SetHotkey(s.hk_mod[2], s.hk_vk[2]);
    m_hk_skin.SetHotkey(s.hk_mod[3], s.hk_vk[3]);
}

PrefsState CMyPreferences::ReadUI()
{
    PrefsState s = m_loaded; // fallback for a control with no selection

    CTrackBarCtrl slider = GetDlgItem(IDC_OPACITY);
    s.opacity = slider.GetPos();
    s.auto_hide = Button_GetCheck(GetDlgItem(IDC_AUTO_HIDE)) == BST_CHECKED;
    s.debug_log = Button_GetCheck(GetDlgItem(IDC_DEBUG_LOG)) == BST_CHECKED;
    s.attached_baseline = Button_GetCheck(GetDlgItem(IDC_ATTACHED_BASELINE)) == BST_CHECKED;

    CComboBox combo = GetDlgItem(IDC_DEFAULT_STYLE);
    int sel = combo.GetCurSel();
    if (sel >= 0) s.style = sel;
    combo = GetDlgItem(IDC_SKIN);
    sel = combo.GetCurSel();
    if (sel >= 0) s.skin = sel;
    // Liquid Glass decides its own appearance from the backdrop, so the control
    // is disabled there and the stored preference must survive untouched.
    combo = GetDlgItem(IDC_COLOR_MODE);
    if (combo.IsWindowEnabled()) {
        sel = combo.GetCurSel();
        if (sel >= 0) s.color_mode = sel;
    }
    combo = GetDlgItem(IDC_LANGUAGE);
    sel = combo.GetCurSel();
    if (sel >= 0) s.language = sel;

    // Font: item 0 (follow) stores "", anything else stores the face name.
    CComboBox font_combo = GetDlgItem(IDC_FONT_FAMILY);
    sel = font_combo.GetCurSel();
    if (sel == 0) {
        s.font.clear();
    } else if (sel > 0) {
        wchar_t buf[LF_FACESIZE] = {};
        font_combo.GetLBText(sel, buf);
        pfc::stringcvt::string_utf8_from_wide utf8(buf);
        s.font = utf8.get_ptr();
    }

    s.hk_mod[0] = m_hk_drag.get_mod();  s.hk_vk[0] = m_hk_drag.get_vk();
    s.hk_mod[1] = m_hk_vis.get_mod();   s.hk_vk[1] = m_hk_vis.get_vk();
    s.hk_mod[2] = m_hk_style.get_mod(); s.hk_vk[2] = m_hk_style.get_vk();
    s.hk_mod[3] = m_hk_skin.get_mod();  s.hk_vk[3] = m_hk_skin.get_vk();
    return s;
}

// Every interactive control persists right away (live preview), through the
// same PrefsState writer the Apply button uses.
void CMyPreferences::Commit()
{
    if (!m_initialized) return;
    ReadUI().apply();
    OnChanged();
}

void CMyPreferences::OnDebugLogClicked(UINT, int, CWindow)
{
    Commit();
    cfg_var_modern::cfg_bool cfg_debug(cfg_guids::debug_logging, false);
    FB2K_console_formatter() << "Floating Clouds: debug logging " << (cfg_debug.get() ? "ON" : "OFF");
}

void CMyPreferences::OnAttachedBaselineClicked(UINT, int, CWindow)
{
    Commit();
    FloatingCloudsWindow::apply_preferences(); // hot reload
}

void CMyPreferences::OnHScroll(int nCode, int nPos, CScrollBar pScrollBar)
{
    if (!m_initialized) return;
    CTrackBarCtrl slider = GetDlgItem(IDC_OPACITY);
    UpdateOpacityLabel(slider.GetPos());
    Commit();
    FloatingCloudsWindow::apply_preferences(); // hot reload
}

void CMyPreferences::OnAutoHideClicked(UINT, int, CWindow)
{
    Commit();
}

void CMyPreferences::OnStyleChanged(UINT, int, CWindow)
{
    Commit();
    FloatingCloudsWindow::apply_preferences(); // hot reload
}

void CMyPreferences::RefreshColorModeCombo()
{
    CComboBox mode = GetDlgItem(IDC_COLOR_MODE);
    if (!mode) return;

    CComboBox skin = GetDlgItem(IDC_SKIN);
    const int sel = skin ? skin.GetCurSel() : -1;
    const bool glass = (sel == (int)FloatingSkin::LiquidGlass);

    if (glass) {
        mode.ResetContent();
        mode.AddString(tr_color_mode_glass());
        mode.SetCurSel(0);
        mode.EnableWindow(FALSE);
        return;
    }

    mode.EnableWindow(TRUE);
    int want = m_loaded.color_mode;
    if (want < 0 || want >= (int)FloatingColorMode::Count) want = DEFAULT_COLOR_MODE;
    refill_combo(mode, (int)FloatingColorMode::Count,
                 [](int i) { return tr_color_mode(static_cast<FloatingColorMode>(i)); });
    mode.SetCurSel(want);
}

void CMyPreferences::OnSkinChanged(UINT, int, CWindow)
{
    // Refresh the mode control BEFORE committing: switching to Liquid Glass must
    // not overwrite the stored colour mode with the disabled entry's index.
    RefreshColorModeCombo();
    Commit();
    FloatingCloudsWindow::apply_preferences(); // hot reload
}

void CMyPreferences::OnColorModeChanged(UINT, int, CWindow)
{
    Commit();
    FloatingCloudsWindow::apply_preferences(); // hot reload
}

void CMyPreferences::OnLanguageChanged(UINT, int, CWindow)
{
    Commit();
    ReloadLocalizedStrings(); // re-localize the whole page immediately
}

void CMyPreferences::OnFontChanged(UINT, int, CWindow)
{
    Commit();
    FloatingCloudsWindow::apply_preferences(); // hot reload
}

void CMyPreferences::ReloadLocalizedStrings()
{
    // Localized-name combos: rebuild the items, keep the current selection.
    CComboBox style_combo = GetDlgItem(IDC_DEFAULT_STYLE);
    refill_combo(style_combo, (int)FloatingStyle::Count, [](int i) { return tr_style(static_cast<FloatingStyle>(i)); });
    CComboBox skin_combo = GetDlgItem(IDC_SKIN);
    refill_combo(skin_combo, (int)FloatingSkin::Count, [](int i) { return tr_skin(static_cast<FloatingSkin>(i)); });
    CComboBox mode_combo = GetDlgItem(IDC_COLOR_MODE);
    RefreshColorModeCombo();

    // Font combo item 0 follows the UI language; keep the selected face.
    CComboBox font_combo = GetDlgItem(IDC_FONT_FAMILY);
    if (font_combo.GetCount() > 0) {
        int sel = font_combo.GetCurSel();
        font_combo.DeleteString(0);
        font_combo.InsertString(0, tr(L"(Follow foobar2000)", L"(跟随 foobar2000)"));
        font_combo.SetCurSel(sel);
    }

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
    SetDlgItemText(IDC_ATTACHED_BASELINE, tr(L"Attach wave baseline (Visualizer+Cover)", L"音浪基线贴附（可视化+封面）"));
    SetDlgItemText(IDC_GRP_SKIN, tr(L"Skin", L"皮肤"));
    SetDlgItemText(IDC_LBL_DEFAULT_SKIN, tr(L"Default skin:", L"默认皮肤："));
    SetDlgItemText(IDC_LBL_COLOR_MODE, tr(L"Color mode:", L"深浅模式："));
    SetDlgItemText(IDC_LBL_FONT_FAMILY, tr(L"Font family:", L"字体："));
    SetDlgItemText(IDC_LBL_LANGUAGE, tr(L"UI Language:", L"界面语言："));
}

LRESULT CMyPreferences::OnHotkeyCaptured(UINT, WPARAM wParam, LPARAM lParam, BOOL&)
{
    uint32_t mod = HIWORD(wParam);
    uint32_t vk = LOWORD(wParam);
    int ctrl_id = (int)lParam;

    Commit(); // the captured boxes hold the new values

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
    if (!m_initialized) return false;
    return !ReadUI().equals(m_loaded);
}

void CMyPreferences::OnChanged()
{
    m_callback->on_state_changed();
}

void CMyPreferences::apply()
{
    PrefsState ui = ReadUI();
    ui.apply();
    m_loaded = ui; // the page is clean now

    FloatingCloudsWindow::apply_preferences();
    OnChanged();
}

void CMyPreferences::reset()
{
    // Language and debug logging keep their current settings (they are written
    // the moment they change), exactly as before.
    PrefsState defaults = ReadUI();
    defaults.opacity = DEFAULT_OPACITY;
    defaults.auto_hide = DEFAULT_AUTO_HIDE;
    defaults.attached_baseline = false;
    defaults.style = DEFAULT_STYLE;
    defaults.skin = DEFAULT_SKIN;
    defaults.color_mode = DEFAULT_COLOR_MODE;
    defaults.font.clear();
    for (int i = 0; i < 4; i++) {
        defaults.hk_mod[i] = k_hk_mod_defaults[i];
        defaults.hk_vk[i] = k_hk_vk_defaults[i];
    }
    SetControls(defaults);

    // Hotkeys have to take effect right away; all other keys wait for Apply.
    write_hotkeys(defaults);
    FloatingCloudsWindow::reload_hotkeys();

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

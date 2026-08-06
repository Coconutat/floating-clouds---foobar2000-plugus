#include "stdafx.h"
#include "resource.h"
#include "preferences.h"
#include "config.h"
#include "DarkMode.h"
#include "atl-misc.h"

// ============================================================================
// Floating Clouds Preferences page
// Uses the standard preferences_page_impl pattern from the SDK
// ============================================================================

// GUID for our preferences page
static constexpr GUID guid_preferences_page = { 0x2a3b4c5d, 0x6e7f, 0x8190, { 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x01 } };

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
        COMMAND_HANDLER_EX(IDC_DEFAULT_STYLE, CBN_SELCHANGE, OnStyleChanged)
    END_MSG_MAP()

private:
    BOOL OnInitDialog(CWindow, LPARAM);
    void OnHScroll(int nCode, int nPos, CScrollBar pScrollBar);
    void OnAutoHideClicked(UINT, int, CWindow);
    void OnStyleChanged(UINT, int, CWindow);
    bool HasChanged();
    void OnChanged();
    void UpdateOpacityLabel(int opacity);

    const preferences_page_callback::ptr m_callback;
    bool m_initialized = false;

    // Dark mode support
    fb2k::CDarkModeHooks m_dark;
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

    // Style combo
    CComboBox style_combo = GetDlgItem(IDC_DEFAULT_STYLE);
    style_combo.AddString(L"Mini");
    style_combo.AddString(L"Mini Art");
    style_combo.AddString(L"Full");
    style_combo.AddString(L"Minimal Line");
    style_combo.AddString(L"Album Focus");
    style_combo.AddString(L"Progress Ring");
    style_combo.AddString(L"Visualizer");
    style_combo.AddString(L"Lyrics Line");
    style_combo.SetCurSel((int)cfg_style.get_value());

    // Hotkey display (read-only)
    SetDlgItemText(IDC_HOTKEY_DRAG, L"Scroll Lock");
    SetDlgItemText(IDC_HOTKEY_VISIBILITY, L"Ctrl+Alt+F");
    SetDlgItemText(IDC_HOTKEY_STYLE, L"Ctrl+Alt+S");

    m_initialized = true;
    return FALSE;
}

void CMyPreferences::OnHScroll(int nCode, int nPos, CScrollBar pScrollBar)
{
    if (!m_initialized) return;
    CTrackBarCtrl slider = GetDlgItem(IDC_OPACITY);
    int pos = slider.GetPos();
    UpdateOpacityLabel(pos);
    OnChanged();
}

void CMyPreferences::OnAutoHideClicked(UINT, int, CWindow)
{
    OnChanged();
}

void CMyPreferences::OnStyleChanged(UINT, int, CWindow)
{
    OnChanged();
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

    CTrackBarCtrl slider = GetDlgItem(IDC_OPACITY);
    int opacity = slider.GetPos();

    bool auto_hide = Button_GetCheck(GetDlgItem(IDC_AUTO_HIDE)) == BST_CHECKED;
    CComboBox style_combo = GetDlgItem(IDC_DEFAULT_STYLE);
    int style = style_combo.GetCurSel();

    return opacity != (int)cfg_opacity.get_value() ||
           auto_hide != cfg_auto_hide.get() ||
           (style >= 0 && style != (int)cfg_style.get_value());
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
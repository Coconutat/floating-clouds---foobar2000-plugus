#include "stdafx.h"
#include "config.h"
#include "localization.h"
#include "resource.h"
#include "atl-misc.h" // preferences_page_impl
#include "DarkMode.h" // fb2k::CDarkModeHooks (dark theme support)

// ============================================================================
// Preferences page: UI language switch (English / Simplified Chinese)
// ============================================================================

static constexpr GUID guid_preferences_page = { 0x7c3d2b1a, 0x4e5f, 0x6071, { 0x82, 0x93, 0xa4, 0xb5, 0xc6, 0xd7, 0xe8, 0x31 } };

class CMyPreferences : public CDialogImpl<CMyPreferences>, public preferences_page_instance {
public:
    CMyPreferences(preferences_page_callback::ptr callback) : m_callback(callback) {}

    enum { IDD = IDD_PREFERENCES };

    t_uint32 get_state() override {
        t_uint32 state = preferences_state::resettable | preferences_state::dark_mode_supported;
        return state;
    }

    void apply() override {}
    void reset() override {
        cfg_var_modern::cfg_int cfg_lang(cfg_guids::language, DEFAULT_LANGUAGE);
        cfg_lang = DEFAULT_LANGUAGE;
        CComboBox lang_combo = GetDlgItem(IDC_LANGUAGE);
        lang_combo.SetCurSel(DEFAULT_LANGUAGE);
        ReloadLocalizedStrings();
        OnChanged();
    }

    BEGIN_MSG_MAP_EX(CMyPreferences)
        MSG_WM_INITDIALOG(OnInitDialog)
        COMMAND_HANDLER_EX(IDC_LANGUAGE, CBN_SELCHANGE, OnLanguageChanged)
    END_MSG_MAP()

private:
    BOOL OnInitDialog(CWindow, LPARAM);
    void OnLanguageChanged(UINT, int, CWindow);
    void ReloadLocalizedStrings();
    void OnChanged() { m_callback->on_state_changed(); }

    const preferences_page_callback::ptr m_callback;
    bool m_initialized = false;
    // Follows the foobar2000 theme's dark mode for this dialog.
    fb2k::CDarkModeHooks m_dark;
};

BOOL CMyPreferences::OnInitDialog(CWindow, LPARAM)
{
    m_dark.AddDialogWithControls(*this); // dark theme support
    // Language combo: English / 中文
    CComboBox lang_combo = GetDlgItem(IDC_LANGUAGE);
    lang_combo.AddString(L"English");
    lang_combo.AddString(L"\x4E2D\x6587"); // 中文
    cfg_var_modern::cfg_int cfg_lang(cfg_guids::language, DEFAULT_LANGUAGE);
    int sel = (int)cfg_lang.get_value();
    if (sel < 0 || sel > 1) sel = 0;
    lang_combo.SetCurSel(sel);

    ReloadLocalizedStrings();
    m_initialized = true;
    return FALSE;
}

void CMyPreferences::OnLanguageChanged(UINT, int, CWindow)
{
    CComboBox lang_combo = GetDlgItem(IDC_LANGUAGE);
    int sel = lang_combo.GetCurSel();
    if (sel < 0) return;
    cfg_var_modern::cfg_int cfg_lang(cfg_guids::language, DEFAULT_LANGUAGE);
    cfg_lang = sel;
    ReloadLocalizedStrings(); // re-localize the page immediately
    OnChanged();
}

void CMyPreferences::ReloadLocalizedStrings()
{
    SetDlgItemText(IDC_GRP_LANGUAGE, tr(L"Language", L"语言"));
    SetDlgItemText(IDC_LBL_LANGUAGE, tr(L"UI Language:", L"界面语言："));
}

// preferences_page_impl<> is defined in atl-misc.h - it inherits from preferences_page_v3.
class preferences_page_myimpl : public preferences_page_impl<CMyPreferences> {
public:
    const char* get_name() { return "Playlist Organizer"; }
    GUID get_guid() { return guid_preferences_page; }
    GUID get_parent_guid() { return preferences_page::guid_components; }
};

static preferences_page_factory_t<preferences_page_myimpl> g_preferences_page_factory;

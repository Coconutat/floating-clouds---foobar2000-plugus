#include "stdafx.h"
#include "config.h"
#include "localization.h"
#include "resource.h"
#include "atl-misc.h"  // preferences_page_impl
#include "DarkMode.h"  // fb2k::CDarkModeHooks

// ============================================================================
// Preferences page: default region, overwrite default, UI language.
// ============================================================================

static constexpr GUID guid_preferences_page = { 0x8e2d4b6a, 0x3f1c, 0x5a79, { 0x90, 0x81, 0x72, 0x63, 0x54, 0x45, 0x36, 0x31 } };

class CMyPreferences : public CDialogImpl<CMyPreferences>, public preferences_page_instance {
public:
    CMyPreferences(preferences_page_callback::ptr callback) : m_callback(callback) {}

    enum { IDD = IDD_PREFERENCES };

    t_uint32 get_state() override {
        return preferences_state::resettable | preferences_state::dark_mode_supported;
    }

    void apply() override {}
    void reset() override {
        cfg_var_modern::cfg_int cfg_lang(cfg_guids::language, DEFAULT_LANGUAGE);
        cfg_lang = DEFAULT_LANGUAGE;
        cfg_var_modern::cfg_int cfg_region(cfg_guids::default_region, DEFAULT_REGION);
        cfg_region = DEFAULT_REGION;
        cfg_var_modern::cfg_bool cfg_ovw(cfg_guids::overwrite_default, DEFAULT_OVERWRITE);
        cfg_ovw = DEFAULT_OVERWRITE;

        CComboBox lang = GetDlgItem(IDC_LANGUAGE);
        lang.SetCurSel(DEFAULT_LANGUAGE);
        CComboBox reg = GetDlgItem(IDC_DEFAULT_REGION);
        reg.SetCurSel(DEFAULT_REGION);
        CheckDlgButton(IDC_OVERWRITE_DEFAULT, DEFAULT_OVERWRITE ? BST_CHECKED : BST_UNCHECKED);
        ReloadLocalizedStrings();
        OnChanged();
    }

    BEGIN_MSG_MAP_EX(CMyPreferences)
        MSG_WM_INITDIALOG(OnInitDialog)
        COMMAND_HANDLER_EX(IDC_LANGUAGE, CBN_SELCHANGE, OnLanguageChanged)
        COMMAND_HANDLER_EX(IDC_DEFAULT_REGION, CBN_SELCHANGE, OnRegionChanged)
        COMMAND_HANDLER_EX(IDC_OVERWRITE_DEFAULT, BN_CLICKED, OnOverwriteChanged)
    END_MSG_MAP()

private:
    BOOL OnInitDialog(CWindow, LPARAM);
    void OnLanguageChanged(UINT, int, CWindow);
    void OnRegionChanged(UINT, int, CWindow);
    void OnOverwriteChanged(UINT, int, CWindow);
    void ReloadLocalizedStrings();
    void PopulateRegionCombo(CComboBox& region);
    void OnChanged() { m_callback->on_state_changed(); }

    const preferences_page_callback::ptr m_callback;
    bool m_initialized = false;
    fb2k::CDarkModeHooks m_dark;
};

BOOL CMyPreferences::OnInitDialog(CWindow, LPARAM)
{
    m_dark.AddDialogWithControls(*this);

    // Region combo
    CComboBox region = GetDlgItem(IDC_DEFAULT_REGION);
    PopulateRegionCombo(region);
    cfg_var_modern::cfg_int cfg_region(cfg_guids::default_region, DEFAULT_REGION);
    int sel = (int)cfg_region.get_value();
    if (sel < 0 || sel >= kRegionCount) sel = DEFAULT_REGION;
    region.SetCurSel(sel);

    // Language combo
    CComboBox lang = GetDlgItem(IDC_LANGUAGE);
    lang.AddString(L"English");
    lang.AddString(L"\x4E2D\x6587"); // 中文
    cfg_var_modern::cfg_int cfg_lang(cfg_guids::language, DEFAULT_LANGUAGE);
    int lang_sel = (int)cfg_lang.get_value();
    if (lang_sel < 0 || lang_sel > 1) lang_sel = 0;
    lang.SetCurSel(lang_sel);

    // Overwrite default
    cfg_var_modern::cfg_bool cfg_ovw(cfg_guids::overwrite_default, DEFAULT_OVERWRITE);
    CheckDlgButton(IDC_OVERWRITE_DEFAULT, cfg_ovw.get() ? BST_CHECKED : BST_UNCHECKED);

    ReloadLocalizedStrings();
    m_initialized = true;
    return FALSE;
}

void CMyPreferences::PopulateRegionCombo(CComboBox& region)
{
    region.ResetContent();
    for (int i = 0; i < kRegionCount; i++) {
        cfg_var_modern::cfg_int lang(cfg_guids::language, DEFAULT_LANGUAGE);
        region.AddString((lang.get_value() == (int64_t)PluginLanguage::Chinese) ? kRegions[i].label_zh : kRegions[i].label);
    }
}

void CMyPreferences::OnLanguageChanged(UINT, int, CWindow)
{
    CComboBox lang = GetDlgItem(IDC_LANGUAGE);
    int sel = lang.GetCurSel();
    if (sel < 0) return;
    cfg_var_modern::cfg_int cfg_lang(cfg_guids::language, DEFAULT_LANGUAGE);
    cfg_lang = sel;
    ReloadLocalizedStrings();
    OnChanged();
}

void CMyPreferences::OnRegionChanged(UINT, int, CWindow)
{
    CComboBox region = GetDlgItem(IDC_DEFAULT_REGION);
    int sel = region.GetCurSel();
    if (sel < 0) return;
    cfg_var_modern::cfg_int cfg_region(cfg_guids::default_region, DEFAULT_REGION);
    cfg_region = sel;
    OnChanged();
}

void CMyPreferences::OnOverwriteChanged(UINT, int, CWindow)
{
    cfg_var_modern::cfg_bool cfg_ovw(cfg_guids::overwrite_default, DEFAULT_OVERWRITE);
    cfg_ovw = (IsDlgButtonChecked(IDC_OVERWRITE_DEFAULT) == BST_CHECKED);
    OnChanged();
}

void CMyPreferences::ReloadLocalizedStrings()
{
    SetDlgItemText(IDC_GRP_DEFAULTS, tr(L"Defaults", L"默认"));
    SetDlgItemText(IDC_LBL_DEFAULT_REGION, tr(L"Default region:", L"默认地区："));
    SetDlgItemText(IDC_OVERWRITE_DEFAULT, tr(L"Overwrite existing tags by default", L"默认覆写已有标签"));
    SetDlgItemText(IDC_GRP_LANGUAGE, tr(L"Language", L"语言"));
    SetDlgItemText(IDC_LBL_LANGUAGE, tr(L"UI Language:", L"界面语言："));

    // Region labels are language-dependent; refill preserving the selection.
    if (m_initialized) {
        CComboBox region = GetDlgItem(IDC_DEFAULT_REGION);
        int sel = region.GetCurSel();
        PopulateRegionCombo(region);
        region.SetCurSel(sel);
    }
}

// preferences_page_impl<> is defined in atl-misc.h.
class preferences_page_myimpl : public preferences_page_impl<CMyPreferences> {
public:
    const char* get_name() { return "Apple Music Tags"; }
    GUID get_guid() { return guid_preferences_page; }
    GUID get_parent_guid() { return preferences_page::guid_components; }
};

static preferences_page_factory_t<preferences_page_myimpl> g_preferences_page_factory;

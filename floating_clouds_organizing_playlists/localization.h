#pragma once

#include "stdafx.h"
#include "config.h"

// ============================================================================
// Localization - English / Simplified Chinese, switchable from Preferences.
// Usage (dialog): SetDlgItemText(id, tr(L"English", L"中文"));
// Usage (menu/message, UTF-8): p_out = tr8("English", "中文");
// ============================================================================

enum class PluginLanguage : int32_t {
    English = 0,
    Chinese = 1,
};

// Wide-char variant for dialog controls.
inline const wchar_t* tr(const wchar_t* en, const wchar_t* zh)
{
    cfg_var_modern::cfg_int lang(cfg_guids::language, DEFAULT_LANGUAGE);
    return (lang.get_value() == (int64_t)PluginLanguage::Chinese) ? zh : en;
}

// UTF-8 variant for menu items and popup messages (pfc::string8 / char*).
inline pfc::string8 tr8(const char* en, const char* zh)
{
    cfg_var_modern::cfg_int lang(cfg_guids::language, DEFAULT_LANGUAGE);
    if (lang.get_value() == (int64_t)PluginLanguage::Chinese) return pfc::string8(zh);
    return pfc::string8(en);
}

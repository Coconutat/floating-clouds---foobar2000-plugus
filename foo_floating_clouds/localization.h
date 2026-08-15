#pragma once

#include "stdafx.h"
#include "config.h"

// ============================================================================
// Localization - English / Simplified Chinese, switchable from Preferences.
// Usage: SetDlgItemText(id, tr(L"English", L"中文"));
// ============================================================================

enum class PluginLanguage : int32_t {
    English = 0,
    Chinese = 1,
};

// Returns the string for the currently configured language.
inline const wchar_t* tr(const wchar_t* en, const wchar_t* zh)
{
    cfg_var_modern::cfg_int lang(cfg_guids::language, DEFAULT_LANGUAGE);
    return (lang.get_value() == (int64_t)PluginLanguage::Chinese) ? zh : en;
}

// Localized name of a style.
inline const wchar_t* tr_style(FloatingStyle s)
{
    switch (s) {
        case FloatingStyle::Minimal: return tr(L"Minimal", L"极简");
        case FloatingStyle::Full: return tr(L"Full", L"完整");
        case FloatingStyle::AlbumFocus: return tr(L"Album Focus", L"专辑焦点");
        case FloatingStyle::ProgressRing: return tr(L"Progress Ring", L"进度环");
        case FloatingStyle::Visualizer: return tr(L"Visualizer", L"可视化");
        case FloatingStyle::LyricsLine: return tr(L"Lyrics Line", L"歌词行");
        default: return L"";
    }
}

// Localized name of a skin.
inline const wchar_t* tr_skin(FloatingSkin s)
{
    switch (s) {
        case FloatingSkin::MD3: return tr(L"Material 3", L"Material 3");
        case FloatingSkin::Apple: return tr(L"Apple (Liquid Glass)", L"Apple（液态玻璃）");
        default: return L"";
    }
}

// Localized name of a color mode.
inline const wchar_t* tr_color_mode(FloatingColorMode m)
{
    switch (m) {
        case FloatingColorMode::Follow: return tr(L"Follow foobar2000", L"跟随 foobar2000");
        case FloatingColorMode::Dark: return tr(L"Dark", L"深色");
        case FloatingColorMode::Light: return tr(L"Light", L"浅色");
        default: return L"";
    }
}

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
        case FloatingStyle::Mini: return tr(L"Mini", L"极简");
        case FloatingStyle::MiniArt: return tr(L"Mini Art", L"小方块");
        case FloatingStyle::Full: return tr(L"Full", L"完整");
        case FloatingStyle::MinimalLine: return tr(L"Minimal Line", L"极简线");
        case FloatingStyle::AlbumFocus: return tr(L"Album Focus", L"专辑焦点");
        case FloatingStyle::ProgressRing: return tr(L"Progress Ring", L"进度环");
        case FloatingStyle::Visualizer: return tr(L"Visualizer", L"可视化");
        case FloatingStyle::LyricsLine: return tr(L"Lyrics Line", L"歌词行");
        default: return L"";
    }
}

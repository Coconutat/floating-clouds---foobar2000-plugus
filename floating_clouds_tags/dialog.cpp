#include "stdafx.h"
#include "dialog.h"
#include "config.h"
#include "localization.h"
#include <SDK/threaded_process.h>

// ============================================================================
// Main fetch/apply dialog implementation
// ============================================================================

namespace {

struct FieldControl {
    int id;
    uint32_t bit;
    const wchar_t* en;
    const wchar_t* zh;
};

const FieldControl kFieldControls[] = {
    { IDC_FIELD_TITLE,       FieldTitle,       L"Title",         L"标题" },
    { IDC_FIELD_ALBUM,       FieldAlbum,       L"Album",         L"专辑" },
    { IDC_FIELD_ARTIST,      FieldArtist,      L"Artist",        L"艺人" },
    { IDC_FIELD_ALBUM_ARTIST, FieldAlbumArtist, L"Album Artist", L"专辑艺人" },
    { IDC_FIELD_GENRE,       FieldGenre,       L"Genre",         L"流派" },
    { IDC_FIELD_DATE,        FieldDate,        L"Release date",  L"发行日期" },
    { IDC_FIELD_TRACK_NO,    FieldTrackNo,     L"Track #",       L"曲目号" },
    { IDC_FIELD_DISC_NO,     FieldDiscNo,      L"Disc #",        L"碟号" },
    { IDC_FIELD_EXPLICIT,    FieldExplicit,    L"Explicit",      L"Explicit" },
};

const wchar_t* region_label(int idx)
{
    if (idx < 0 || idx >= kRegionCount) idx = DEFAULT_REGION;
    cfg_var_modern::cfg_int lang(cfg_guids::language, DEFAULT_LANGUAGE);
    return (lang.get_value() == (int64_t)PluginLanguage::Chinese) ? kRegions[idx].label_zh : kRegions[idx].label;
}

int region_index_by_code(const char* code)
{
    if (!code) return -1;
    for (int i = 0; i < kRegionCount; i++) {
        if (pfc::stricmp_ascii(kRegions[i].code, code) == 0) return i;
    }
    return -1;
}

} // namespace

BOOL TagsDialog::OnInitDialog(CWindow, LPARAM)
{
    m_dark.AddDialogWithControls(*this); // dark theme support

    // Region combo
    CComboBox region = GetDlgItem(IDC_REGION);
    for (int i = 0; i < kRegionCount; i++) region.AddString(region_label(i));
    cfg_var_modern::cfg_int cfg_region(cfg_guids::default_region, DEFAULT_REGION);
    m_region_index = (int)cfg_region.get_value();
    if (m_region_index < 0 || m_region_index >= kRegionCount) m_region_index = DEFAULT_REGION;
    region.SetCurSel(m_region_index);

    // Field checkboxes: everything checked except Explicit
    for (const FieldControl& fc : kFieldControls) {
        CheckDlgButton(fc.id, (fc.bit == FieldExplicit) ? BST_UNCHECKED : BST_CHECKED);
    }

    // Overwrite toggle
    cfg_var_modern::cfg_bool cfg_ovw(cfg_guids::overwrite_default, DEFAULT_OVERWRITE);
    CheckDlgButton(IDC_OVERWRITE, cfg_ovw.get() ? BST_CHECKED : BST_UNCHECKED);

    // HK->CN conversion toggle: off and disabled until an album is fetched.
    CheckDlgButton(IDC_CONVERT_SC, BST_UNCHECKED);
    ::EnableWindow(GetDlgItem(IDC_CONVERT_SC), FALSE);

    m_region_touched = false;
    PrefillFromClipboard();
    ReloadStrings();
    ApplyEnabled(false);
    return FALSE;
}

void TagsDialog::ReloadStrings()
{
    SetWindowText(tr(L"Apple Music Tags", L"Apple Music 标签更新"));
    SetDlgItemText(IDC_FETCH_HINT, tr(
        L"Paste a music.apple.com album link or numeric album ID (clipboard is prefilled if it holds one).",
        L"粘贴 music.apple.com 专辑链接或数字专辑 ID（剪贴板里有链接会自动预填）。"));
    SetDlgItemText(IDC_FETCH, tr(L"Fetch", L"获取"));
    SetDlgItemText(IDC_APPLY, tr(L"Apply", L"应用"));
    SetDlgItemText(IDC_CLOSE, tr(L"Close", L"关闭"));
    SetDlgItemText(IDC_OVERWRITE, tr(L"Overwrite existing tags", L"覆写已有标签"));
    SetDlgItemText(IDC_FORCE_ORDER, tr(L"Force write in selection order", L"强制按选择顺序写入（紧急）"));
    SetDlgItemText(IDC_CONVERT_SC, tr(L"Convert to Simplified Chinese (HK->CN)", L"转为简体中文（港→简）"));
    for (const FieldControl& fc : kFieldControls) {
        SetDlgItemText(fc.id, tr(fc.en, fc.zh));
    }
}

void TagsDialog::PrefillFromClipboard()
{
    if (!::OpenClipboard(m_hWnd)) return;
    HANDLE h = ::GetClipboardData(CF_UNICODETEXT);
    if (!h) { ::CloseClipboard(); return; }
    const wchar_t* text = (const wchar_t*)::GlobalLock(h);
    if (text) {
        const wchar_t* p = text;
        while (*p == L' ' || *p == L'\t' || *p == L'\r' || *p == L'\n') p++;
        size_t len = wcslen(p);
        while (len > 0 && (p[len - 1] == L' ' || p[len - 1] == L'\t' || p[len - 1] == L'\r' || p[len - 1] == L'\n')) len--;
        if (len > 0) {
            std::wstring w(p, len);
            SetDlgItemTextW(IDC_URL, w.c_str());
            pfc::string8 region;
            int album_id = 0;
            if (parse_album_url(w.c_str(), region, album_id) && !region.is_empty()) {
                int idx = region_index_by_code(region.get_ptr());
                if (idx >= 0) {
                    CComboBox cb = GetDlgItem(IDC_REGION);
                    cb.SetCurSel(idx);
                    m_region_index = idx;
                }
            }
        }
        ::GlobalUnlock(h);
    }
    ::CloseClipboard();
}

bool TagsDialog::FetchNow(int album_id, int region_index, AppleAlbum& out, pfc::string8& err)
{
    const char* code = kRegions[region_index].code;
    pfc::string8 fetch_error;
    bool aborted = false;

    service_ptr_t<threaded_process_callback> cb = threaded_process_callback_lambda::create(
        [&](threaded_process_status&, abort_callback& a) {
            try {
                fetch_album_auto(album_id, code, a, out);
            } catch (const exception_aborted&) {
                aborted = true;
            } catch (const pfc::exception& e) {
                fetch_error = e.what();
            } catch (...) {
                fetch_error = "Unknown error.";
            }
        });

    threaded_process::g_run_modal(cb,
        threaded_process::flag_show_abort | threaded_process::flag_show_progress,
        m_hWnd, tr8("Fetching album from Apple Music...", "正在从 Apple Music 获取专辑…"));

    if (aborted) { err = tr8("Fetch aborted.", "获取已取消。"); return false; }
    if (!out.ok) { err = out.error.is_empty() ? fetch_error : out.error; return false; }
    return true;
}

void TagsDialog::OnFetch(UINT, int, CWindow)
{
    HWND h = GetDlgItem(IDC_URL);
    int len = ::GetWindowTextLengthW(h);
    std::wstring w;
    if (len > 0) {
        w.resize((size_t)len + 1);
        ::GetWindowTextW(h, &w[0], len + 1);
        w.resize((size_t)len);
    }

    pfc::string8 region;
    int album_id = 0;
    if (!parse_album_url(w.c_str(), region, album_id)) {
        popup_message::g_show(
            tr8("Enter a valid Apple Music album link or numeric album ID.",
                "请输入有效的 Apple Music 专辑链接或数字专辑 ID。"),
            tr8("Apple Music Tags", "Apple Music 标签更新"));
        return;
    }

    // Region: the combo selection wins. The URL's region only provides the
    // default before the user has touched the dropdown (pasting a
    // region-bearing link must not override an explicit choice).
    int region_index = m_region_index;
    if (!region.is_empty() && !m_region_touched) {
        int idx = region_index_by_code(region.get_ptr());
        if (idx >= 0) region_index = idx;
    }
    CComboBox cb = GetDlgItem(IDC_REGION);
    cb.SetCurSel(region_index);
    m_region_index = region_index;

    AppleAlbum album;
    pfc::string8 err;
    if (!FetchNow(album_id, region_index, album, err)) {
        popup_message::g_show(err, tr8("Apple Music Tags", "Apple Music 标签更新"));
        return;
    }

    m_album_original = album;
    m_album = album;
    m_has_album = true;
    // The HK->CN toggle mirrors whether the auto CN-fallback already converted.
    CheckDlgButton(IDC_CONVERT_SC, m_album.converted_from_hk ? BST_CHECKED : BST_UNCHECKED);
    UpdatePreview();
    ApplyEnabled(true);

    // CN->HK fallback: tell the user plainly that these are char-by-char
    // converted (Traditional->Simplified) HK tags, not official CN metadata.
    if (m_album.converted_from_hk) {
        popup_message::g_show(
            tr8("This album is not available on the CN storefront. Tags were fetched "
                "from the HK storefront and converted character-by-character from "
                "Traditional Chinese to Simplified Chinese. Some titles or names may "
                "differ from mainland conventions.",
                "该专辑在 CN 地区不存在。已改从港版获取标签，并将繁体中文逐字转换为简体中文。"
                "个别标题或译名可能与大陆习惯不同。"),
            tr8("Apple Music Tags", "Apple Music 标签更新"));
    }
}

void TagsDialog::UpdatePreview()
{
    if (!m_has_album) { SetDlgItemText(IDC_PREVIEW, L""); return; }

    pfc::stringcvt::string_wide_from_utf8 name(m_album.album_name);
    pfc::stringcvt::string_wide_from_utf8 artist(m_album.album_artist);

    std::wstring preview;
    preview += name;
    preview += L"\r\n";
    preview += tr(L"Artist: ", L"艺人：");
    preview += artist;
    preview += L"    ";
    preview += tr(L"Region: ", L"地区：");
    preview += region_label(m_region_index);
    if (m_album.converted_from_hk) {
        preview += tr(L" (converted from HK Traditional Chinese, char-by-char, not official Simplified)",
                      L"（港版繁体逐字转简，非官方简中）");
    }
    preview += L"\r\n";
    preview += tr(L"Tracks: ", L"曲目数：");
    preview += std::to_wstring((int)m_album.tracks.get_size());
    preview += tr(L" fetched, ", L" 首已获取，");
    preview += std::to_wstring((int)m_selected.get_count());
    preview += tr(L" selected.", L" 首已选中。");

    SetDlgItemTextW(IDC_PREVIEW, preview.c_str());
}

void TagsDialog::OnApply(UINT, int, CWindow)
{
    if (!m_has_album) return;

    TagOptions opt;
    opt.fields = 0;
    for (const FieldControl& fc : kFieldControls) {
        if (IsDlgButtonChecked(fc.id) == BST_CHECKED) opt.fields |= fc.bit;
    }
    opt.overwrite = (IsDlgButtonChecked(IDC_OVERWRITE) == BST_CHECKED);
    opt.force_order = (IsDlgButtonChecked(IDC_FORCE_ORDER) == BST_CHECKED);

    // Remember the chosen region for next time.
    cfg_var_modern::cfg_int cfg_region(cfg_guids::default_region, DEFAULT_REGION);
    cfg_region = m_region_index;

    apply_tags(m_selected, m_album, opt, m_hWnd);
}

void TagsDialog::OnRegionChanged(UINT, int, CWindow)
{
    CComboBox cb = GetDlgItem(IDC_REGION);
    int sel = cb.GetCurSel();
    if (sel < 0) return;
    m_region_touched = true;
    m_region_index = sel;
    UpdatePreview();
}

void TagsDialog::OnClose(UINT, int, CWindow)
{
    EndDialog(IDCANCEL);
}

void TagsDialog::OnConvertToggled(UINT, int, CWindow)
{
    if (!m_has_album) return;
    if (IsDlgButtonChecked(IDC_CONVERT_SC) == BST_CHECKED) {
        // HK (or any traditional) tags -> Simplified Chinese, char-by-char.
        m_album = m_album_original;
        to_simplified(m_album);
        m_album.converted_from_hk = true;
    } else {
        m_album = m_album_original; // restore the original fetched tags
    }
    UpdatePreview();
}

LRESULT TagsDialog::OnWmClose(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    bHandled = TRUE;
    EndDialog(IDCANCEL);
    return 0;
}

void TagsDialog::ApplyEnabled(bool enabled)
{
    ::EnableWindow(GetDlgItem(IDC_APPLY), enabled ? TRUE : FALSE);
    ::EnableWindow(GetDlgItem(IDC_CONVERT_SC), enabled ? TRUE : FALSE);
}

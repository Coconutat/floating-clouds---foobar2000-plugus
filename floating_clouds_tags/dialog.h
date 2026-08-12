#pragma once

#include "stdafx.h"
#include "resource.h"
#include "itunes.h"
#include "tagger.h"
#include "config.h" // DEFAULT_REGION
#include "DarkMode.h" // fb2k::CDarkModeHooks

// ============================================================================
// Modal dialog: paste album URL, pick region, fetch, preview, apply.
// ============================================================================

class TagsDialog : public CDialogImpl<TagsDialog> {
public:
    enum { IDD = IDD_MAIN };

    explicit TagsDialog(const metadb_handle_list& selected) : m_selected(selected) {}

    BEGIN_MSG_MAP_EX(TagsDialog)
        MSG_WM_INITDIALOG(OnInitDialog)
        COMMAND_ID_HANDLER_EX(IDC_FETCH, OnFetch)
        COMMAND_ID_HANDLER_EX(IDC_APPLY, OnApply)
        COMMAND_ID_HANDLER_EX(IDC_CLOSE, OnClose)
        COMMAND_HANDLER_EX(IDC_REGION, CBN_SELCHANGE, OnRegionChanged)
        COMMAND_HANDLER_EX(IDC_CONVERT_SC, BN_CLICKED, OnConvertToggled)
        MESSAGE_HANDLER(WM_CLOSE, OnWmClose)
    END_MSG_MAP()

private:
    BOOL OnInitDialog(CWindow wndFocus, LPARAM lInitParam);
    void OnFetch(UINT uNotifyCode, int nID, CWindow wndCtl);
    void OnApply(UINT uNotifyCode, int nID, CWindow wndCtl);
    void OnClose(UINT uNotifyCode, int nID, CWindow wndCtl);
    void OnRegionChanged(UINT uNotifyCode, int nID, CWindow wndCtl);
    void OnConvertToggled(UINT uNotifyCode, int nID, CWindow wndCtl);
    LRESULT OnWmClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

    void ReloadStrings();
    void PrefillFromClipboard();
    void UpdatePreview();
    bool FetchNow(int album_id, int region_index, AppleAlbum& out, pfc::string8& err);
    void ApplyEnabled(bool enabled);

    metadb_handle_list m_selected;
    AppleAlbum m_album;          // tags that will be applied
    AppleAlbum m_album_original; // pre-conversion copy (for the HK->CN toggle)
    bool m_has_album = false;
    bool m_region_touched = false; // user manually changed the region combo
    int m_region_index = DEFAULT_REGION;
    fb2k::CDarkModeHooks m_dark;
};

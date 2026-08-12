#include "stdafx.h"
#include "playlist_panel.h"
#include "localization.h"
#include <map>
#include <vector>
#include <algorithm>
#include <imm.h>
#pragma comment(lib, "imm32.lib")

// ============================================================================
// PlaylistPanel implementation
// ============================================================================

namespace {
constexpr int PAD = 10;
constexpr int HEADER_H = 40;
constexpr int SEARCH_H = 34;
constexpr int ROW_H = 28;
constexpr int SCROLL_W = 4;
constexpr int BTN_W = 32; // back / close button zone width
constexpr int LETTER_W = 24;   // right-edge A-Z / 0-9 / # index bar
constexpr int LETTER_COUNT = 37; // 26 + 10 + '#'

// Case-insensitive (ASCII only) substring search over UTF-8.
bool contains_ci(const pfc::string8& hay, const pfc::string8& needle)
{
    if (needle.is_empty()) return true;
    const char* h = hay.get_ptr();
    const t_size hl = hay.length();
    const char* n = needle.get_ptr();
    const t_size nl = needle.length();
    if (nl > hl) return false;
    for (t_size i = 0; i + nl <= hl; i++) {
        bool ok = true;
        for (t_size j = 0; j < nl; j++) {
            char a = h[i + j], b = n[j];
            if (a >= 'A' && a <= 'Z') a += 32;
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) { ok = false; break; }
        }
        if (ok) return true;
    }
    return false;
}

// Drop the last UTF-8 character.
void utf8_pop_back(pfc::string8& s)
{
    const t_size len = s.length();
    if (len == 0) return;
    t_size i = len;
    while (i > 0 && ((unsigned char)s[i - 1] & 0xC0) == 0x80) i--;
    if (i > 0) i--; // the leading byte
    s.truncate(i);
}
} // namespace

PlaylistPanel::PlaylistPanel()
{
    titleformat_compiler::get()->compile(m_script_song, "[%album% - ]%title%");
}

PlaylistPanel::~PlaylistPanel()
{
}

LRESULT PlaylistPanel::OnCreate(LPCREATESTRUCT cs)
{
    D2DRenderer::initialize(m_hWnd);
    // Both present modes use WS_EX_LAYERED (ULW presents via UpdateLayeredWindow).
    DWORD ex_style = FLOATING_WINDOW_EX_STYLE | WS_EX_LAYERED;
    SetWindowLong(GWL_EXSTYLE, ex_style);
    return 0;
}

void PlaylistPanel::OnDestroy()
{
    stop_marquee_timer();
    D2DRenderer::release_resources();
    SetMsgHandled(FALSE);
}

void PlaylistPanel::open(HWND parent, const CPoint& screen_pos)
{
    if (!IsWindow()) {
        Create(parent); // top-level layered window, owned by the floating window
    }
    if (m_open) { // toggle: clicking the button again closes it
        close();
        return;
    }

    m_level = Level::Playlists;
    load_playlists();
    m_search.reset();
    m_comp.reset();
    m_composing = false;
    m_filter_letter = -1;
    rebuild_view();
    m_scroll = 0.0f;
    m_hover_row = -1;
    m_size = CSize(PANEL_WIDTH, PANEL_HEIGHT);

    SetWindowPos(NULL, screen_pos.x, screen_pos.y, PANEL_WIDTH, PANEL_HEIGHT,
                 SWP_NOZORDER | SWP_SHOWWINDOW);
    m_open = true;
    SetFocus();
    Invalidate();
}

void PlaylistPanel::close()
{
    if (!m_open) return;
    m_open = false;
    m_hover_row = -1;
    stop_marquee_timer();
    if (IsWindow()) ShowWindow(SW_HIDE);
}

BOOL PlaylistPanel::OnEraseBkgnd(CDCHandle dc)
{
    return TRUE; // handled in OnPaint
}

void PlaylistPanel::OnSize(UINT nType, CSize size)
{
    m_size = size;
    on_resize(size);
}

void PlaylistPanel::OnPaint(CDCHandle dc)
{
    CPaintDC paint_dc(m_hWnd);
    if (!begin_draw()) return;

    const bool ulw = is_ulw();
    const float inset = ulw ? (float)SHADOW_INSET : 0.0f;
    const float hdr_y = inset;
    const float hdr_h = (float)HEADER_H;
    const float W = (float)m_size.cx;

    if (ulw) {
        clear_background(0.0f);
        draw_surface_card(D2D1::RectF(inset, inset, W - inset, (float)m_size.cy - inset),
                          (float)WINDOW_CORNER_RADIUS);
    } else {
        clear_background(1.0f);
    }

    // Header background (rounded top corners in ULW mode to follow the card)
    m_brush->SetColor(D2DRenderer::hex(md3::surface_container_high, 1.0f));
    if (ulw) {
        m_render_target->FillRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(inset, hdr_y, W - inset, hdr_y + hdr_h),
                              (float)WINDOW_CORNER_RADIUS, (float)WINDOW_CORNER_RADIUS), m_brush);
        m_render_target->FillRectangle(
            D2D1::RectF(inset, hdr_y + (float)WINDOW_CORNER_RADIUS, W - inset, hdr_y + hdr_h), m_brush);
    } else {
        m_render_target->FillRectangle(D2D1::RectF(0, 0, W, hdr_h), m_brush);
    }

    // Header title depends on the current level. Horizontally centered in the
    // free space (between the back/close zones on the albums/tracks levels,
    // full width on the playlists level), vertically centered in the header.
    const wchar_t* title;
    pfc::stringcvt::string_wide_from_utf8 w_playlist(m_playlist_name);
    pfc::stringcvt::string_wide_from_utf8 w_album(m_album_name);
    switch (m_level) {
        case Level::Playlists: title = tr(L"Playlists", L"播放列表"); break;
        case Level::Albums: title = (const wchar_t*)w_playlist; break;
        default: title = (const wchar_t*)w_album; break;
    }
    const float title_left = (m_level == Level::Playlists) ? (float)PAD : (float)(PAD + BTN_W);
    const float title_right = W - (float)(BTN_W + PAD);
    const float tw = measure_text_width(title, get_title_format());
    const float tx = title_left + (std::max)(0.0f, (title_right - title_left - tw) / 2.0f);
    const float font_size = get_title_format()->GetFontSize();
    const float ty = hdr_y + (hdr_h - font_size) / 2.0f - 1.0f;
    m_brush->SetColor(D2DRenderer::hex(md3::on_surface, 0.95f));
    D2D1_RECT_F title_rect = D2D1::RectF(tx, ty, title_right, ty + font_size + 4.0f);
    m_render_target->DrawText(title, (UINT32)wcslen(title), get_title_format(), title_rect, m_brush,
                              D2D1_DRAW_TEXT_OPTIONS_CLIP);

    // Back button (albums / tracks levels)
    if (m_level != Level::Playlists) {
        m_brush->SetColor(D2DRenderer::hex(md3::on_surface_variant, 0.9f));
        D2D1_RECT_F back_rect = D2D1::RectF((float)PAD, hdr_y, (float)(PAD + BTN_W), hdr_y + hdr_h);
        m_render_target->DrawText(L"\u2190", 1, get_title_format(), back_rect, m_brush,
                                  D2D1_DRAW_TEXT_OPTIONS_NONE);
    }

    // Close button
    m_brush->SetColor(D2DRenderer::hex(md3::on_surface_variant, 0.9f));
    D2D1_RECT_F close_rect = D2D1::RectF(W - (float)(BTN_W + PAD), hdr_y, W - (float)PAD, hdr_y + hdr_h);
    m_render_target->DrawText(L"\u00D7", 1, get_title_format(), close_rect, m_brush,
                              D2D1_DRAW_TEXT_OPTIONS_NONE);

    // Search row (all levels)
    const float sy = hdr_y + hdr_h;
    const float sxl = (float)list_left() + 4.0f;
    const float sxr = (float)list_right();
    draw_search_box(sxl, sy + 3.0f, (std::max)(20.0f, sxr - sxl - 8.0f), (float)SEARCH_H - 6.0f);

    // List rows (only the visible slice of the filtered view)
    const float list_top = (float)content_top();
    const float list_left_f = (float)list_left();
    const float text_w = (float)list_right();
    const t_size rows = m_view.get_size();
    for (t_size i = 0; i < rows; i++) {
        const float ry = list_top - m_scroll + (float)i * (float)ROW_H;
        if (ry + ROW_H < 0 || ry > m_size.cy) continue;

        if (m_hover_row >= 0 && (t_size)m_hover_row == i) {
            m_brush->SetColor(D2DRenderer::hex(md3::on_surface, md3::hover_state));
            m_render_target->FillRectangle(D2D1::RectF(inset, ry, W - inset, ry + ROW_H), m_brush);
        }

        const pfc::string8& text = active_name(m_view[i]);
        pfc::stringcvt::string_wide_from_utf8 wtext(text);
        if (m_hover_row >= 0 && (t_size)m_hover_row == i) {
            // Hovered row auto-scrolls (marquee) so long titles stay readable.
            draw_text(wtext, list_left_f, ry, text_w - list_left_f - (float)PAD, (float)ROW_H,
                      get_title_format(), D2DRenderer::hex(md3::on_surface, 0.9f));
        } else {
            m_brush->SetColor(D2DRenderer::hex(md3::on_surface, 0.9f));
            D2D1_RECT_F tr = D2D1::RectF(list_left_f, ry, text_w, ry + ROW_H);
            m_render_target->DrawText((const wchar_t*)wtext, (UINT32)wcslen((const wchar_t*)wtext),
                                      get_title_format(), tr, m_brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
    }

    // No matches placeholder
    if (rows == 0) {
        const wchar_t* none = tr(L"No matches", L"无匹配");
        m_brush->SetColor(D2DRenderer::hex(md3::on_surface_variant, 0.6f));
        D2D1_RECT_F nr = D2D1::RectF(list_left_f, list_top + 24.0f, text_w, list_top + 48.0f);
        m_render_target->DrawText(none, (UINT32)wcslen(none), get_small_format(), nr, m_brush,
                                  D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // Letter index bar (playlists / albums levels)
    if (letter_bar_visible()) {
        const float lx = (float)letter_bar_x();
        const float top = list_top;
        const float bot = (float)(m_size.cy - (ulw ? SHADOW_INSET : 0));
        const float per = (bot - top) / (float)LETTER_COUNT;
        const char* glyphs = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789#";
        for (int i = 0; i < LETTER_COUNT; i++) {
            const float gy = top + (float)i * per;
            const bool active = (m_filter_letter == i);
            const bool present = m_letter_present[i];
            if (active) {
                m_brush->SetColor(D2DRenderer::hex(md3::primary, 0.18f));
                m_render_target->FillRectangle(D2D1::RectF(lx, gy, W - inset, gy + per), m_brush);
            }
            m_brush->SetColor(present
                ? D2DRenderer::hex(active ? md3::primary : md3::on_surface_variant, active ? 1.0f : 0.85f)
                : D2DRenderer::hex(md3::on_surface_variant, 0.30f));
            const wchar_t ch = (wchar_t)glyphs[i];
            D2D1_RECT_F cr = D2D1::RectF(lx, gy + per * 0.15f, W - inset, gy + per);
            m_render_target->DrawText(&ch, 1, get_small_format(), cr, m_brush, D2D1_DRAW_TEXT_OPTIONS_NONE);
        }
    }

    // Scrollbar (only when the content overflows)
    const float max_sc = max_scroll();
    if (max_sc > 0.0f) {
        const float view_h = (float)(m_size.cy - content_top() - (ulw ? SHADOW_INSET : 0));
        const float content_h = rows * (float)ROW_H;
        float thumb_h = view_h * view_h / content_h;
        if (thumb_h < 20.0f) thumb_h = 20.0f;
        const float thumb_y = (float)content_top() + (m_scroll / max_sc) * (view_h - thumb_h);
        const float sb_x = letter_bar_visible()
            ? (float)(letter_bar_x() - SCROLL_W - 2)
            : (W - inset - (float)(SCROLL_W + 2));
        m_brush->SetColor(D2DRenderer::hex(md3::on_surface_variant, 0.6f));
        m_render_target->FillRectangle(D2D1::RectF(sb_x, thumb_y, sb_x + (float)SCROLL_W, thumb_y + thumb_h), m_brush);
    }

    end_draw();

    // Keep the frame loop alive while any row is scrolling (marquee).
    if (is_marquee_active()) {
        start_marquee_timer();
    } else {
        stop_marquee_timer();
    }
}

LRESULT PlaylistPanel::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    const float step = (float)ROW_H * 3.0f;
    m_scroll -= (float)zDelta / 120.0f * step;
    m_scroll = (std::max)(0.0f, (std::min)(m_scroll, max_scroll()));
    Invalidate();
    return 0;
}

void PlaylistPanel::OnMouseMove(UINT nFlags, CPoint point)
{
    const int row = row_at(point);
    if (row != m_hover_row) {
        m_hover_row = row;
        Invalidate();
    }
    if (row >= 0) {
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd, 0 };
        TrackMouseEvent(&tme);
    }
}

LRESULT PlaylistPanel::OnMouseLeave(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    if (m_hover_row != -1) { m_hover_row = -1; Invalidate(); }
    stop_marquee_timer();
    bHandled = TRUE;
    return 0;
}

void PlaylistPanel::OnLButtonDown(UINT nFlags, CPoint point)
{
    // Focus the search box when clicking it (typing immediately works).
    if (in_search_row(point)) {
        focus_search();
        return;
    }
    // Clicking anywhere else drops the search focus (the text is kept).
    if (m_search_focused) unfocus_search();
}

void PlaylistPanel::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (hit_close(point)) { close(); return; }

    // Search box: clear button
    if (in_search_row(point)) {
        if (hit_search_clear(point)) update_search(pfc::string8());
        return;
    }

    // Back button (albums / tracks levels)
    if (m_level != Level::Playlists && hit_back(point)) {
        enter_level(m_level == Level::Tracks ? Level::Albums : Level::Playlists);
        return;
    }

    // Letter index bar (playlists / albums levels): click = filter toggle
    const int letter = letter_at(point);
    if (letter >= 0) {
        if (!m_letter_present[letter]) return; // dimmed / empty
        m_filter_letter = (m_filter_letter == letter) ? -1 : letter;
        rebuild_view();
        m_scroll = 0.0f;
        m_hover_row = -1;
        Invalidate();
        return;
    }

    const int row = row_at(point);
    if (row < 0) return;

    const t_size view_idx = m_view[(t_size)row];
    switch (m_level) {
        case Level::Playlists:
            m_playlist_name = m_playlists[view_idx].name;
            load_albums(m_playlists[view_idx].index);
            enter_level(Level::Albums);
            break;
        case Level::Albums:
            m_album_name = m_albums[view_idx].name;
            load_tracks(view_idx);
            enter_level(Level::Tracks);
            break;
        default:
            play_track(m_tracks[view_idx].item_index);
            close();
            break;
    }
}

void PlaylistPanel::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == VK_ESCAPE) {
        if (m_composing) return; // let the IME handle ESC during composition
        close();
    }
}

void PlaylistPanel::OnChar(UINT nChar, UINT, UINT)
{
    if (!m_search_focused || m_composing) return;
    if (nChar == '\b') { backspace(); return; }
    if (nChar < 0x20) return;
    const wchar_t wc = (wchar_t)nChar;
    append_text(&wc, 1);
}

LRESULT PlaylistPanel::OnImeStartComposition(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    if (!m_search_focused) { bHandled = FALSE; return 0; }
    m_composing = true;
    m_comp.reset();
    setup_ime_window();
    Invalidate();
    bHandled = TRUE;
    return 1; // we handle composition ourselves
}

LRESULT PlaylistPanel::OnImeComposition(UINT, WPARAM, LPARAM lParam, BOOL& bHandled)
{
    if (!m_search_focused) { bHandled = FALSE; return 0; }
    HIMC himc = ImmGetContext(m_hWnd);
    if (himc) {
        if (lParam & GCS_RESULTSTR) {
            const int len = ImmGetCompositionStringW(himc, GCS_RESULTSTR, NULL, 0);
            if (len > 0) {
                pfc::array_t<wchar_t> buf;
                buf.set_size((t_size)len / 2 + 1);
                ImmGetCompositionStringW(himc, GCS_RESULTSTR, buf.get_ptr(), len);
                buf[(t_size)len / 2] = 0;
                append_text(buf.get_ptr(), len / 2);
            }
        }
        if (lParam & GCS_COMPSTR) {
            const int len = ImmGetCompositionStringW(himc, GCS_COMPSTR, NULL, 0);
            if (len > 0) {
                pfc::array_t<wchar_t> buf;
                buf.set_size((t_size)len / 2 + 1);
                ImmGetCompositionStringW(himc, GCS_COMPSTR, buf.get_ptr(), len);
                buf[(t_size)len / 2] = 0;
                pfc::stringcvt::string_utf8_from_wide w(buf.get_ptr());
                m_comp = (const char*)w;
            } else {
                m_comp.reset();
            }
        }
        ImmReleaseContext(m_hWnd, himc);
    }
    Invalidate();
    bHandled = TRUE;
    return 0;
}

LRESULT PlaylistPanel::OnImeEndComposition(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    if (!m_search_focused) { bHandled = FALSE; return 0; }
    m_composing = false;
    m_comp.reset();
    Invalidate();
    bHandled = TRUE;
    return 0;
}

void PlaylistPanel::OnActivate(UINT nState, BOOL bMinimized, CWindow wndOther)
{
    // Clicking anywhere outside the panel deactivates it -> close.
    if (nState == WA_INACTIVE) close();
}

LRESULT PlaylistPanel::OnTimer(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    // Redraw to advance any scrolling (marquee) row.
    Invalidate();
    bHandled = TRUE;
    return 0;
}

void PlaylistPanel::start_marquee_timer()
{
    if (m_marquee_timer == 0) {
        m_marquee_timer = SetTimer(1, 16, NULL);
    }
}

void PlaylistPanel::stop_marquee_timer()
{
    if (m_marquee_timer != 0) {
        KillTimer(1);
        m_marquee_timer = 0;
    }
}

int PlaylistPanel::row_at(CPoint point) const
{
    if (point.y < content_top()) return -1;
    const int rows = (int)m_view.get_size();
    const int idx = (int)((point.y - content_top() + m_scroll) / ROW_H);
    if (idx < 0 || idx >= rows) return -1;
    return idx;
}

bool PlaylistPanel::in_header(CPoint point) const
{
    return point.y < content_top();
}

bool PlaylistPanel::in_search_row(CPoint point) const
{
    const int top = content_top();
    return point.y >= top - SEARCH_H && point.y < top;
}

bool PlaylistPanel::hit_back(CPoint point) const
{
    const int lx = is_ulw() ? SHADOW_INSET : 0;
    return m_level != Level::Playlists && point.y < content_top() - SEARCH_H && point.x < lx + PAD + BTN_W;
}

bool PlaylistPanel::hit_close(CPoint point) const
{
    const int lx = is_ulw() ? SHADOW_INSET : 0;
    return point.y < content_top() - SEARCH_H && point.x > m_size.cx - lx - PAD - BTN_W;
}

bool PlaylistPanel::hit_search_clear(CPoint point) const
{
    if (m_search.is_empty()) return false;
    if (!in_search_row(point)) return false;
    const int lx = is_ulw() ? SHADOW_INSET : 0;
    return point.x > m_size.cx - lx - PAD - 24;
}

int PlaylistPanel::letter_bar_x() const
{
    return m_size.cx - (is_ulw() ? SHADOW_INSET : 0) - LETTER_W;
}

int PlaylistPanel::letter_at(CPoint point) const
{
    if (!letter_bar_visible()) return -1;
    if (point.x < letter_bar_x()) return -1;
    const int top = content_top();
    const int bot = m_size.cy - (is_ulw() ? SHADOW_INSET : 0);
    if (point.y < top || point.y >= bot) return -1;
    const int idx = (point.y - top) * LETTER_COUNT / (bot - top);
    if (idx < 0 || idx >= LETTER_COUNT) return -1;
    return idx;
}

int PlaylistPanel::content_top() const
{
    return (is_ulw() ? SHADOW_INSET : 0) + HEADER_H + SEARCH_H;
}

int PlaylistPanel::list_left() const
{
    return (is_ulw() ? SHADOW_INSET : 0) + PAD;
}

int PlaylistPanel::list_right() const
{
    const int inset = is_ulw() ? SHADOW_INSET : 0;
    const int bar_w = letter_bar_visible() ? (LETTER_W + SCROLL_W + 4) : (SCROLL_W + 4);
    return m_size.cx - inset - bar_w - PAD;
}

t_size PlaylistPanel::active_count() const
{
    switch (m_level) {
        case Level::Playlists: return m_playlists.get_size();
        case Level::Albums: return m_albums.get_size();
        default: return m_tracks.get_size();
    }
}

const pfc::string8& PlaylistPanel::active_name(t_size i) const
{
    switch (m_level) {
        case Level::Playlists: return m_playlists[i].name;
        case Level::Albums: return m_albums[i].name;
        default: return m_tracks[i].label;
    }
}

float PlaylistPanel::max_scroll() const
{
    const int rows = (int)m_view.get_size();
    const int inset = is_ulw() ? SHADOW_INSET : 0;
    const float view_h = (float)(m_size.cy - content_top() - inset);
    const float content_h = rows * (float)ROW_H;
    return content_h > view_h ? content_h - view_h : 0.0f;
}

void PlaylistPanel::load_playlists()
{
    m_playlists.remove_all();
    auto pm = playlist_manager::get();
    const t_size n = pm->get_playlist_count();

    std::vector<PList> vec;
    vec.reserve(n);
    for (t_size i = 0; i < n; i++) {
        PList pl;
        pl.index = i;
        pm->playlist_get_name(i, pl.name);
        vec.push_back(pl);
    }
    // Sort: index bucket (A-Z, 0-9, #) first, then zh-CN collation within a bucket.
    std::stable_sort(vec.begin(), vec.end(), [](const PList& a, const PList& b) {
        const int ka = pinyin::index_key(a.name);
        const int kb = pinyin::index_key(b.name);
        if (ka != kb) return ka < kb;
        return pinyin::compare_zh(a.name, b.name) < 0;
    });
    for (const PList& pl : vec) m_playlists.add_item(pl);
}

void PlaylistPanel::load_albums(t_size playlist_idx)
{
    m_albums.remove_all();
    m_items.remove_all();
    m_tracks.remove_all();
    m_sel_playlist = playlist_idx;
    m_sel_album = pfc::infinite_size;

    auto pm = playlist_manager::get();
    const t_size n = pm->playlist_get_item_count(playlist_idx);

    // Read per-item info and group by (album, album artist).
    std::vector<ItemInfo> items;
    items.reserve(n);
    std::map<pfc::string8, std::vector<t_size>> groups;
    for (t_size j = 0; j < n; j++) {
        ItemInfo it;
        metadb_handle_ptr h;
        if (pm->playlist_get_item_handle(h, playlist_idx, j) && h.is_valid()) {
            metadb_info_container::ptr info;
            if (h->get_info_ref(info)) {
                const file_info& fi = info->info();
                if (const char* s = fi.meta_get("ALBUM", 0)) it.album = s;
                if (const char* s = fi.meta_get("ALBUM ARTIST", 0)) it.artist = s;
                if (const char* s = fi.meta_get("TITLE", 0)) it.title = s;
                if (const char* s = fi.meta_get("TRACKNUMBER", 0)) { it.track = atoi(s); it.has_track = it.track > 0; }
                if (const char* s = fi.meta_get("DISCNUMBER", 0)) { const int d = atoi(s); if (d > 0) it.disc = d; }
            }
        }
        pfc::string8 key = it.album;
        key << "\x01" << it.artist;
        groups[key].push_back(j);
        items.push_back(it);
    }
    for (const ItemInfo& it : items) m_items.add_item(it);

    // Build albums; sort each album's tracks by (disc, track).
    std::vector<Album> album_vec;
    album_vec.reserve(groups.size());
    for (auto& kv : groups) {
        Album al;
        const t_size first = kv.second[0];
        al.name = items[first].album;
        al.artist = items[first].artist;
        std::vector<t_size> idxs = kv.second;
        std::sort(idxs.begin(), idxs.end(), [&](t_size a, t_size b) {
            const ItemInfo& ia = items[a];
            const ItemInfo& ib = items[b];
            if (ia.disc != ib.disc) return ia.disc < ib.disc;
            if (ia.has_track || ib.has_track) {
                if (ia.track != ib.track) return ia.track < ib.track;
            }
            return a < b;
        });
        for (const t_size idx : idxs) al.items.add_item(idx);
        album_vec.push_back(al);
    }
    // Sort albums by name bucket then zh collation; empty name ("(No Album)") -> '#' last.
    std::stable_sort(album_vec.begin(), album_vec.end(), [](const Album& a, const Album& b) {
        const int ka = pinyin::index_key(a.name);
        const int kb = pinyin::index_key(b.name);
        if (ka != kb) return ka < kb;
        return pinyin::compare_zh(a.name, b.name) < 0;
    });
    for (const Album& al : album_vec) m_albums.add_item(al);
}

void PlaylistPanel::load_tracks(t_size album_idx)
{
    m_tracks.remove_all();
    m_sel_album = album_idx;
    const Album& al = m_albums[album_idx];
    for (t_size k = 0; k < al.items.get_size(); k++) {
        const t_size idx = al.items[k];
        const ItemInfo& it = m_items[idx];
        TrackEntry te;
        te.item_index = idx;
        pfc::string8 label;
        if (it.has_track) {
            if (it.disc > 1) label << it.disc << "-";
            label << it.track << ". ";
        }
        label << it.title;
        te.label = label;
        m_tracks.add_item(te);
    }
}

void PlaylistPanel::play_track(t_size item_idx)
{
    auto pm = playlist_manager::get();
    pm->set_active_playlist(m_sel_playlist);
    pm->playlist_execute_default_action(m_sel_playlist, item_idx);
}

void PlaylistPanel::enter_level(Level lv)
{
    m_level = lv;
    // Search + letter filter reset on every level change (fresh context).
    m_search.reset();
    m_comp.reset();
    m_composing = false;
    m_filter_letter = -1;
    rebuild_view();
    m_scroll = 0.0f;
    m_hover_row = -1;
    Invalidate();
}

void PlaylistPanel::rebuild_view()
{
    m_view.remove_all();
    for (int i = 0; i < LETTER_COUNT; i++) m_letter_present[i] = false;
    const t_size n = active_count();
    for (t_size i = 0; i < n; i++) {
        const pfc::string8& name = active_name(i);
        if (!m_search.is_empty() && !contains_ci(name, m_search)) continue;
        const int k = pinyin::index_key(name);
        if (m_filter_letter >= 0 && k != m_filter_letter) continue;
        m_view.add_item(i);
        if (k >= 0 && k < LETTER_COUNT) m_letter_present[k] = true;
    }
    m_scroll = (std::min)(m_scroll, max_scroll());
}

void PlaylistPanel::focus_search()
{
    m_search_focused = true;
    SetFocus();
    setup_ime_window();
    Invalidate();
}

void PlaylistPanel::unfocus_search()
{
    m_search_focused = false;
    m_composing = false;
    m_comp.reset();
    Invalidate();
}

void PlaylistPanel::update_search(const pfc::string8& text)
{
    m_search = text;
    rebuild_view();
    Invalidate();
}

void PlaylistPanel::append_text(const wchar_t* s, int len)
{
    if (len <= 0) return;
    wchar_t tmp[8];
    if (len > 7) len = 7;
    for (int i = 0; i < len; i++) tmp[i] = s[i];
    tmp[len] = 0;
    pfc::stringcvt::string_utf8_from_wide w(tmp);
    m_search += (const char*)w;
    rebuild_view();
    Invalidate();
}

void PlaylistPanel::backspace()
{
    if (m_search.is_empty()) return;
    utf8_pop_back(m_search);
    rebuild_view();
    Invalidate();
}

void PlaylistPanel::setup_ime_window()
{
    HIMC himc = ImmGetContext(m_hWnd);
    if (!himc) return;
    const int inset = is_ulw() ? SHADOW_INSET : 0;
    const int sy = inset + HEADER_H + SEARCH_H / 2;
    COMPOSITIONFORM cf = {};
    cf.dwStyle = CFS_POINT;
    cf.ptCurrentPos.x = inset + PAD + 6;
    cf.ptCurrentPos.y = sy;
    ImmSetCompositionWindow(himc, &cf);
    CANDIDATEFORM cand = {};
    cand.dwStyle = CFS_EXCLUDE;
    cand.ptCurrentPos.x = inset + PAD;
    cand.ptCurrentPos.y = inset + HEADER_H;
    cand.rcArea = { inset + PAD, inset + HEADER_H, m_size.cx - inset - PAD, inset + HEADER_H + SEARCH_H };
    ImmSetCandidateWindow(himc, &cand);
    ImmReleaseContext(m_hWnd, himc);
}

void PlaylistPanel::draw_search_box(float x, float y, float w, float h)
{
    const float r = h / 2.0f;
    // Field background
    m_brush->SetColor(D2DRenderer::hex(md3::surface_container_highest, 1.0f));
    m_render_target->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + h), r, r), m_brush);

    const float tx = x + 10.0f;
    const float tw = (std::max)(20.0f, w - 44.0f); // leave room for the clear button

    pfc::string8 display = m_search;
    if (m_composing) display += m_comp;

    if (!display.is_empty()) {
        pfc::stringcvt::string_wide_from_utf8 wtext(display);
        m_brush->SetColor(D2DRenderer::hex(md3::on_surface, 0.95f));
        D2D1_RECT_F tr = D2D1::RectF(tx, y, tx + tw, y + h);
        m_render_target->DrawText((const wchar_t*)wtext, (UINT32)wcslen((const wchar_t*)wtext),
                                  get_small_left_format(), tr, m_brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        // Caret when focused
        if (m_search_focused) {
            const float cw = measure_text_width((const wchar_t*)wtext, get_small_left_format());
            const float cx = (std::min)(tx + cw, tx + tw);
            m_brush->SetColor(D2DRenderer::hex(md3::primary, 1.0f));
            m_render_target->FillRectangle(D2D1::RectF(cx, y + 4.0f, cx + 1.5f, y + h - 4.0f), m_brush);
        }
    } else if (!m_search_focused) {
        const wchar_t* ph = tr(L"Search...", L"搜索…");
        m_brush->SetColor(D2DRenderer::hex(md3::on_surface_variant, 0.6f));
        D2D1_RECT_F tr = D2D1::RectF(tx, y, tx + tw, y + h);
        m_render_target->DrawText(ph, (UINT32)wcslen(ph), get_small_left_format(), tr, m_brush,
                                  D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // Clear button
    if (!m_search.is_empty()) {
        m_brush->SetColor(D2DRenderer::hex(md3::on_surface_variant, 0.9f));
        D2D1_RECT_F cr = D2D1::RectF(x + w - 22.0f, y, x + w - 4.0f, y + h);
        m_render_target->DrawText(L"\u00D7", 1, get_small_format(), cr, m_brush, D2D1_DRAW_TEXT_OPTIONS_NONE);
    }
}

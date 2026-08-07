#include "stdafx.h"
#include "playlist_panel.h"

// ============================================================================
// PlaylistPanel implementation
// ============================================================================

namespace {
constexpr int PAD = 10;
constexpr int HEADER_H = 40;
constexpr int ROW_H = 28;
constexpr int SCROLL_W = 4;
constexpr int BTN_W = 32; // back / close button zone width
}

PlaylistPanel::PlaylistPanel()
{
    // Track labels in the track view: "Album - Title" (album omitted when empty).
    titleformat_compiler::get()->compile(m_script_song, "[%album% - ]%title%");
}

PlaylistPanel::~PlaylistPanel()
{
}

LRESULT PlaylistPanel::OnCreate(LPCREATESTRUCT cs)
{
    D2DRenderer::initialize(m_hWnd);
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

    load_playlists();
    m_in_tracks = false;
    m_scroll = 0.0f;
    m_hover_row = -1;
    m_size = CSize(PANEL_WIDTH, PANEL_HEIGHT);

    ::SetLayeredWindowAttributes(m_hWnd, 0, 255, LWA_ALPHA);
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
    if (m_render_target) {
        m_render_target->Resize(D2D1::SizeU(size.cx, size.cy));
    }
}

void PlaylistPanel::OnPaint(CDCHandle dc)
{
    CPaintDC paint_dc(m_hWnd);
    if (!begin_draw()) return;

    // Opaque MD3 surface (Clear alpha is ignored under LWA_ALPHA uniform alpha).
    clear_background(1.0f);

    const int rows = m_in_tracks ? (int)m_tracks.get_size() : (int)m_playlists.get_size();
    const float W = (float)m_size.cx;

    // Header background
    m_brush->SetColor(D2DRenderer::hex(md3::surface_container_high, 1.0f));
    m_render_target->FillRectangle(D2D1::RectF(0, 0, W, (float)HEADER_H), m_brush);

    // Header title
    const char* title = "Playlists";
    if (m_in_tracks && m_sel_playlist < m_playlists.get_size())
        title = m_playlists[m_sel_playlist];
    pfc::stringcvt::string_wide_from_utf8 wtitle(title);
    m_brush->SetColor(D2DRenderer::hex(md3::on_surface, 0.95f));
    D2D1_RECT_F title_rect = D2D1::RectF((float)(PAD + BTN_W), 0, W - (float)(BTN_W + PAD), (float)HEADER_H);
    m_render_target->DrawText((const wchar_t*)wtitle, (UINT32)wcslen((const wchar_t*)wtitle),
                              get_title_format(), title_rect, m_brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);

    // Back button (track view)
    if (m_in_tracks) {
        m_brush->SetColor(D2DRenderer::hex(md3::on_surface_variant, 0.9f));
        D2D1_RECT_F back_rect = D2D1::RectF((float)PAD, 0, (float)(PAD + BTN_W), (float)HEADER_H);
        m_render_target->DrawText(L"\u2190", 1, get_title_format(), back_rect, m_brush,
                                  D2D1_DRAW_TEXT_OPTIONS_NONE);
    }

    // Close button
    m_brush->SetColor(D2DRenderer::hex(md3::on_surface_variant, 0.9f));
    D2D1_RECT_F close_rect = D2D1::RectF(W - (float)(BTN_W + PAD), 0, W - (float)PAD, (float)HEADER_H);
    m_render_target->DrawText(L"\u00D7", 1, get_title_format(), close_rect, m_brush,
                              D2D1_DRAW_TEXT_OPTIONS_NONE);

    // List rows (only the visible slice)
    for (int i = 0; i < rows; i++) {
        const float ry = (float)HEADER_H - m_scroll + i * (float)ROW_H;
        if (ry + ROW_H < 0 || ry > m_size.cy) continue;

        if (i == m_hover_row) {
            m_brush->SetColor(D2DRenderer::hex(md3::on_surface, md3::hover_state));
            m_render_target->FillRectangle(D2D1::RectF(0, ry, W, ry + ROW_H), m_brush);
        }

        const char* text = m_in_tracks ? (const char*)m_tracks[i] : (const char*)m_playlists[i];
        pfc::stringcvt::string_wide_from_utf8 wtext(text);
        const float text_w = W - (float)(PAD + SCROLL_W + 4);
        if (i == m_hover_row) {
            // Hovered row auto-scrolls (marquee) so long titles stay readable.
            draw_text(wtext, (float)PAD, ry, text_w - (float)PAD, (float)ROW_H,
                      get_title_format(), D2DRenderer::hex(md3::on_surface, 0.9f));
        } else {
            m_brush->SetColor(D2DRenderer::hex(md3::on_surface, 0.9f));
            D2D1_RECT_F tr = D2D1::RectF((float)PAD, ry, text_w, ry + ROW_H);
            m_render_target->DrawText((const wchar_t*)wtext, (UINT32)wcslen((const wchar_t*)wtext),
                                      get_title_format(), tr, m_brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
    }

    // Scrollbar (only when the content overflows)
    const float max_sc = max_scroll();
    if (max_sc > 0.0f) {
        const float view_h = (float)m_size.cy - HEADER_H - PAD;
        const float content_h = (float)HEADER_H + rows * (float)ROW_H + PAD;
        float thumb_h = view_h * view_h / content_h;
        if (thumb_h < 20.0f) thumb_h = 20.0f;
        const float thumb_y = (float)HEADER_H + (m_scroll / max_sc) * (view_h - thumb_h);
        m_brush->SetColor(D2DRenderer::hex(md3::on_surface_variant, 0.6f));
        m_render_target->FillRectangle(
            D2D1::RectF(W - (float)(SCROLL_W + 2), thumb_y, W - 2.0f, thumb_y + thumb_h), m_brush);
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
    // (Selection is performed on release for consistency with the floating window.)
}

void PlaylistPanel::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (hit_close(point)) { close(); return; }
    if (m_in_tracks && hit_back(point)) {
        m_in_tracks = false;
        m_scroll = 0.0f;
        m_hover_row = -1;
        Invalidate();
        return;
    }

    const int row = row_at(point);
    if (row < 0) return;

    if (!m_in_tracks) {
        load_tracks((t_size)row);
        m_in_tracks = true;
        m_scroll = 0.0f;
        m_hover_row = -1;
        Invalidate();
    } else {
        play_track(m_sel_playlist, (t_size)row);
        close();
    }
}

void PlaylistPanel::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == VK_ESCAPE) close();
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
    if (point.y < HEADER_H) return -1;
    const int rows = m_in_tracks ? (int)m_tracks.get_size() : (int)m_playlists.get_size();
    const int idx = (int)((point.y - HEADER_H + m_scroll) / ROW_H);
    if (idx < 0 || idx >= rows) return -1;
    return idx;
}

bool PlaylistPanel::in_header(CPoint point) const
{
    return point.y < HEADER_H;
}

bool PlaylistPanel::hit_back(CPoint point) const
{
    return m_in_tracks && point.y < HEADER_H && point.x < PAD + BTN_W;
}

bool PlaylistPanel::hit_close(CPoint point) const
{
    return point.y < HEADER_H && point.x > m_size.cx - PAD - BTN_W;
}

float PlaylistPanel::max_scroll() const
{
    const int rows = m_in_tracks ? (int)m_tracks.get_size() : (int)m_playlists.get_size();
    const float content_h = (float)HEADER_H + rows * (float)ROW_H + PAD;
    return content_h > (float)m_size.cy ? content_h - (float)m_size.cy : 0.0f;
}

void PlaylistPanel::load_playlists()
{
    m_playlists.remove_all();
    auto pm = playlist_manager::get();
    const t_size n = pm->get_playlist_count();
    for (t_size i = 0; i < n; i++) {
        pfc::string8 name;
        pm->playlist_get_name(i, name);
        m_playlists.add_item(name);
    }
}

void PlaylistPanel::load_tracks(t_size playlist_idx)
{
    m_tracks.remove_all();
    m_sel_playlist = playlist_idx;
    auto pm = playlist_manager::get();
    const t_size n = pm->playlist_get_item_count(playlist_idx);
    for (t_size j = 0; j < n; j++) {
        pfc::string8 title;
        pm->playlist_item_format_title(playlist_idx, j, NULL, title, m_script_song, NULL,
                                       playback_control::display_level_all);
        m_tracks.add_item(title);
    }
}

void PlaylistPanel::play_track(t_size playlist_idx, t_size item_idx)
{
    auto pm = playlist_manager::get();
    pm->set_active_playlist(playlist_idx);
    pm->playlist_execute_default_action(playlist_idx, item_idx);
}

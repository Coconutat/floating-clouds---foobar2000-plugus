#pragma once

#include "stdafx.h"
#include "config.h"
#include "d2d_renderer.h"

// ============================================================================
// PlaylistPanel - a custom D2D list window for picking a playlist, then a
// track to play. A separate top-level layered window with its own render
// target: two-level view (playlists -> tracks), mouse-wheel scrolling and a
// visible scrollbar, hover highlight, back / close buttons.
// ============================================================================

class PlaylistPanel :
    public CWindowImpl<PlaylistPanel, CWindow,
        CWinTraits<WS_POPUP | WS_VISIBLE, WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST>>,
    public D2DRenderer
{
public:
    static constexpr int PANEL_WIDTH = 360;
    static constexpr int PANEL_HEIGHT = 460;

    PlaylistPanel();
    ~PlaylistPanel();

    DECLARE_WND_CLASS_EX(TEXT("FloatingClouds_PlaylistPanel"), CS_VREDRAW | CS_HREDRAW, -1);

    BEGIN_MSG_MAP_EX(PlaylistPanel)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_DESTROY(OnDestroy)
        MSG_WM_PAINT(OnPaint)
        MSG_WM_ERASEBKGND(OnEraseBkgnd)
        MSG_WM_SIZE(OnSize)
        MSG_WM_MOUSEWHEEL(OnMouseWheel)
        MSG_WM_MOUSEMOVE(OnMouseMove)
        MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)
        MSG_WM_LBUTTONDOWN(OnLButtonDown)
        MSG_WM_LBUTTONUP(OnLButtonUp)
        MSG_WM_KEYDOWN(OnKeyDown)
        MSG_WM_ACTIVATE(OnActivate)
        MESSAGE_HANDLER(WM_TIMER, OnTimer)
    END_MSG_MAP()

    // Open (or toggle closed) the panel near the floating window.
    void open(HWND parent, const CPoint& screen_pos);
    void close();
    bool is_open() const { return m_open; }

private:
    LRESULT OnCreate(LPCREATESTRUCT cs);
    void OnDestroy();
    void OnPaint(CDCHandle dc);
    BOOL OnEraseBkgnd(CDCHandle dc);
    void OnSize(UINT nType, CSize size);
    LRESULT OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    void OnMouseMove(UINT nFlags, CPoint point);
    LRESULT OnMouseLeave(UINT, WPARAM, LPARAM, BOOL&);
    void OnLButtonDown(UINT nFlags, CPoint point);
    void OnLButtonUp(UINT nFlags, CPoint point);
    void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    void OnActivate(UINT nState, BOOL bMinimized, CWindow wndOther);
    LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&);
    void start_marquee_timer();
    void stop_marquee_timer();

    void load_playlists();
    void load_tracks(t_size playlist_idx);
    void play_track(t_size playlist_idx, t_size item_idx);

    // Geometry / hit-testing
    int row_at(CPoint point) const;             // row index under point, -1 if none
    bool in_header(CPoint point) const;
    bool hit_back(CPoint point) const;
    bool hit_close(CPoint point) const;
    float max_scroll() const;

    // Data (view 0 = playlists, view 1 = tracks of m_sel_playlist)
    pfc::list_t<pfc::string8> m_playlists;
    pfc::list_t<pfc::string8> m_tracks;
    t_size m_sel_playlist = pfc::infinite_size;
    bool m_in_tracks = false;

    // Scroll offset in pixels
    float m_scroll = 0.0f;

    // Interaction
    int m_hover_row = -1;
    bool m_open = false;
    CSize m_size;
    UINT_PTR m_marquee_timer = 0; // 16ms frame timer while a row is scrolling
    service_ptr_t<titleformat_object> m_script_song;
};

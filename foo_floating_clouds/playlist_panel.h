#pragma once

#include "stdafx.h"
#include "config.h"
#include "d2d_renderer.h"
#include "pinyin.h"

// ============================================================================
// PlaylistPanel - a custom D2D list window for picking a playlist, then an
// album, then a track. A separate top-level layered window with its own render
// target: three-level navigation (playlists -> albums -> tracks), a self-drawn
// search box with IMM (Chinese IME) support, an A-Z / 0-9 / # letter index
// (pinyin for Chinese names), mouse-wheel scrolling and a visible scrollbar,
// hover highlight, back / close buttons.
// ============================================================================

class PlaylistPanel :
    public CWindowImpl<PlaylistPanel, CWindow,
        CWinTraits<WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW | WS_EX_TOPMOST>>,
    public D2DRenderer
{
public:
    static constexpr int PANEL_WIDTH = 360;
    static constexpr int PANEL_HEIGHT = 500;

    enum class Level { Playlists, Albums, Tracks };

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
        MSG_WM_CHAR(OnChar)
        MESSAGE_HANDLER(WM_IME_STARTCOMPOSITION, OnImeStartComposition)
        MESSAGE_HANDLER(WM_IME_COMPOSITION, OnImeComposition)
        MESSAGE_HANDLER(WM_IME_ENDCOMPOSITION, OnImeEndComposition)
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
    void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
    LRESULT OnImeStartComposition(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnImeComposition(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnImeEndComposition(UINT, WPARAM, LPARAM, BOOL&);
    void OnActivate(UINT nState, BOOL bMinimized, CWindow wndOther);
    LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&);
    void start_marquee_timer();
    void stop_marquee_timer();

    // Navigation / data loading
    void load_playlists();
    void load_albums(t_size playlist_idx);
    void load_tracks(t_size album_idx);
    void play_track(t_size item_idx);
    void enter_level(Level lv);
    void rebuild_view();

    // Search + IME
    void focus_search();
    void unfocus_search();
    void update_search(const pfc::string8& text);
    void append_text(const wchar_t* s, int len);
    void backspace();
    void setup_ime_window();
    void draw_search_box(float x, float y, float w, float h);

    // Geometry / hit-testing
    int row_at(CPoint point) const;
    bool in_header(CPoint point) const;
    bool in_search_row(CPoint point) const;
    bool hit_back(CPoint point) const;
    bool hit_close(CPoint point) const;
    bool hit_search_clear(CPoint point) const;
    int letter_at(CPoint point) const;
    bool letter_bar_visible() const { return m_level != Level::Tracks; }
    int letter_bar_x() const;
    float max_scroll() const;
    int content_top() const;
    int list_left() const;
    int list_right() const;
    t_size active_count() const;
    const pfc::string8& active_name(t_size i) const;

    // Data
    struct PList { pfc::string8 name; t_size index; };
    struct Album { pfc::string8 name; pfc::string8 artist; pfc::list_t<t_size> items; };
    struct TrackEntry { pfc::string8 label; t_size item_index; };
    struct ItemInfo { pfc::string8 album, artist, title; int disc = 1, track = 0; bool has_track = false; };

    pfc::list_t<PList> m_playlists;      // sorted by zh collation
    pfc::list_t<ItemInfo> m_items;       // per item of m_sel_playlist (playlist order)
    pfc::list_t<Album> m_albums;         // grouped, sorted by album name
    pfc::list_t<TrackEntry> m_tracks;    // current album's tracks
    t_size m_sel_playlist = pfc::infinite_size;
    t_size m_sel_album = pfc::infinite_size;
    pfc::string8 m_playlist_name;        // header title on the albums level
    pfc::string8 m_album_name;           // header title on the tracks level
    Level m_level = Level::Playlists;

    // Filtered view + filter state
    pfc::list_t<t_size> m_view;          // indices into the active list
    bool m_letter_present[37] = {};      // A-Z (0-25), 0-9 (26-35), '#' (36)
    pfc::string8 m_search;               // committed search text (UTF-8)
    pfc::string8 m_comp;                 // IME composition string (UTF-8)
    bool m_composing = false;
    bool m_search_focused = false;
    int m_filter_letter = -1;            // -1 none; 0-25 A-Z; 26-35 0-9; 36 '#'

    // Scroll offset in pixels
    float m_scroll = 0.0f;

    // Interaction
    int m_hover_row = -1;
    bool m_open = false;
    CSize m_size;
    UINT_PTR m_marquee_timer = 0; // 16ms frame timer while a row is scrolling
    service_ptr_t<titleformat_object> m_script_song;
};

#pragma once

#include "stdafx.h"
#include "config.h"
#include "d2d_renderer.h"
#include "hotkey_manager.h"
#include "playback_listener.h"
#include "tray_icon.h"

// ============================================================================
// FloatingCloudsWindow - The main floating overlay window
// ============================================================================

// NOTE: CWindowImpl<T> defaults to CControlWinTraits, which forces WS_CHILD.
// A WS_CHILD window with no parent is never shown on the desktop, so we must
// explicitly use top-level window traits (WS_POPUP) here.
class FloatingCloudsWindow :
    public CWindowImpl<FloatingCloudsWindow, CWindow,
        CWinTraits<WS_POPUP | WS_VISIBLE, WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_TOPMOST>>,
    public D2DRenderer
{
public:
    FloatingCloudsWindow();
    ~FloatingCloudsWindow();

    DECLARE_WND_CLASS_EX(TEXT("FloatingClouds_Window"), CS_VREDRAW | CS_HREDRAW, -1);

    BEGIN_MSG_MAP_EX(FloatingCloudsWindow)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_DESTROY(OnDestroy)
        MSG_WM_PAINT(OnPaint)
        MSG_WM_ERASEBKGND(OnEraseBkgnd)
        MSG_WM_SIZE(OnSize)
        MSG_WM_NCHITTEST(OnNcHitTest)
        MSG_WM_LBUTTONDOWN(OnLButtonDown)
        MSG_WM_LBUTTONUP(OnLButtonUp)
        MSG_WM_MOUSEMOVE(OnMouseMove)
        MSG_WM_SETCURSOR(OnSetCursor)
        MESSAGE_HANDLER(WM_HOTKEY, OnHotKey)
        MESSAGE_HANDLER(WM_DISPLAYCHANGE, OnDisplayChange)
        MESSAGE_HANDLER(FC_WM_TRAY_NOTIFY, OnTrayNotify)
        MESSAGE_HANDLER(WM_TIMER, OnTimer)
    END_MSG_MAP()

    // Initialize the window
    void initialize_window(HWND parent);

    // Re-register hotkeys from config (called when hotkeys change in Preferences)
    static void reload_hotkeys();

    // Apply opacity/style preferences to the live window immediately (hot reload)
    static void apply_preferences();

    // True if the given pointer is still the live window instance (safe after destruction)
    static bool is_current(FloatingCloudsWindow* w) { return s_instance == w; }
    
    // Show/hide with animation
    void show_with_animation();
    void hide_with_animation();
    
    // Toggle visibility
    void toggle_visibility();
    
    // Toggle drag mode
    void toggle_drag_mode();
    
    // Cycle to next style
    void cycle_style();
    
    // Set a specific style
    void set_style(FloatingStyle style);
    
    // Update playback info (called from playback listener)
    void on_playback_new_track(const char* title, const char* artist, const char* album, 
                               album_art_data_ptr art);
    void on_playback_stop();
    void on_playback_time(double time);
    void on_playback_pause(bool paused);
    void on_volume_change(float volume);
    
    // Getters
    bool is_drag_mode() const { return m_drag_mode; }
    bool is_visible() const { return m_visible; }
    bool is_playing() const { return m_playing; }
    bool is_paused() const { return m_paused; }
    bool is_volume_muted() const { return m_volume <= -99.0f; }
    FloatingStyle get_current_style() const { return m_current_style; }
    const char* get_title() const { return m_title; }
    const char* get_artist() const { return m_artist; }
    const char* get_album() const { return m_album; }
    album_art_data_ptr get_album_art() const { return m_album_art; }
    double get_playback_time() const { return m_playback_time; }
    double get_track_length() const { return m_track_length; }
    float get_volume() const { return m_volume; }
    // Eased progress value shown to the user (0..1), animated by the frame loop
    float get_display_progress() const { return m_display_progress; }

private:
    // Window event handlers
    LRESULT OnCreate(LPCREATESTRUCT cs);
    void OnDestroy();
    void OnPaint(CDCHandle dc);
    BOOL OnEraseBkgnd(CDCHandle dc);
    void OnSize(UINT nType, CSize size);
    LRESULT OnNcHitTest(CPoint point);
    void OnLButtonDown(UINT nFlags, CPoint point);
    void OnLButtonUp(UINT nFlags, CPoint point);
    void OnMouseMove(UINT nFlags, CPoint point);
    BOOL OnSetCursor(CWindow wnd, UINT nHitTest, UINT message);
    LRESULT OnHotKey(UINT msg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnDisplayChange(UINT msg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnTrayNotify(UINT msg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

    // Animation frame loop
    LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&);
    void start_anim_timer();
    void stop_anim_timer();
    void on_anim_tick();

    // Calculate window size based on current style
    CSize calculate_size();
    
    // Update layered window for transparency
    void update_layered_window();

    // Hit test for buttons
    int hit_test_button(CPoint point);

    // State
    bool m_drag_mode = false;
    bool m_visible = true;
    bool m_playing = false;
    bool m_paused = false;
    FloatingStyle m_current_style = static_cast<FloatingStyle>(DEFAULT_STYLE);
    
    // Playback data
    pfc::string8 m_title;
    pfc::string8 m_artist;
    pfc::string8 m_album;
    album_art_data_ptr m_album_art;
    double m_playback_time = 0.0;
    double m_track_length = 0.0;
    float m_volume = 0.0f;
    
    // Drag state
    bool m_dragging = false;
    CPoint m_drag_offset;
    
    // Hotkey manager
    std::unique_ptr<HotkeyManager> m_hotkeys;
    
    // Playback listener
    std::unique_ptr<PlaybackListener> m_playback_listener;
    
    // Tray icon
    std::unique_ptr<TrayIcon> m_tray_icon;
    
    // Animation
    float m_anim_opacity = 1.0f;
    bool m_animating = false;
    float m_display_progress = 0.0f;   // eased value shown to the user
    float m_target_progress = 0.0f;    // real playback progress target
    UINT_PTR m_anim_timer = 0;
    double m_last_anim_tick = 0.0;

    // Process-wide pointer to the active window (so Preferences can reload hotkeys)
    static FloatingCloudsWindow* s_instance;
};
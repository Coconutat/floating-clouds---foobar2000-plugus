#include "stdafx.h"
#include "floating_window.h"
#include "config.h"
#include "styles/style_renderer.h"

#include <SDK/audio_chunk_impl.h>

// ============================================================================
// FloatingCloudsWindow implementation
// ============================================================================

FloatingCloudsWindow* FloatingCloudsWindow::s_instance = nullptr;

FloatingCloudsWindow::FloatingCloudsWindow()
    : m_hotkeys(std::make_unique<HotkeyManager>())
    , m_playback_listener(std::make_unique<PlaybackListener>(this))
    , m_tray_icon(std::make_unique<TrayIcon>(this))
{
}

FloatingCloudsWindow::~FloatingCloudsWindow()
{
    m_tray_icon->destroy();
    m_hotkeys->unregister_all();
}

void FloatingCloudsWindow::initialize_window(HWND parent)
{
    Create(parent);
    s_instance = this;
    FB2K_console_formatter() << "Floating Clouds: Create -> hwnd=" << (t_size)(size_t)m_hWnd
        << " style=0x" << GetWindowLong(GWL_STYLE)
        << " exstyle=0x" << GetWindowLong(GWL_EXSTYLE)
        << " visible=" << (IsWindowVisible() ? 1 : 0);
    
    // Set layered window with transparency
    SetWindowLong(GWL_EXSTYLE, FLOATING_WINDOW_EX_STYLE);
    ::SetLayeredWindowAttributes(m_hWnd, 0, (BYTE)DEFAULT_OPACITY, LWA_ALPHA);
    
    // Initialize D2D renderer
    bool d2d_ok = D2DRenderer::initialize(m_hWnd);
    FB2K_console_formatter() << "Floating Clouds: D2D initialize=" << (d2d_ok ? 1 : 0);
    
    // Register hotkeys
    m_hotkeys->register_all(m_hWnd);
    
    // Create tray icon
    m_tray_icon->create(m_hWnd);
    
    // Calculate initial size and position
    CSize size = calculate_size();
    // Center on screen
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int x = (screen_w - size.cx) / 2;
    int y = screen_h - size.cy - 60; // Near bottom of screen
    
    // Restore saved position
    cfg_var_modern::cfg_int cfg_x(cfg_guids::window_x, x);
    cfg_var_modern::cfg_int cfg_y(cfg_guids::window_y, y);
    cfg_var_modern::cfg_int cfg_style(cfg_guids::current_style, DEFAULT_STYLE);
    
    x = (int)cfg_x.get_value();
    y = (int)cfg_y.get_value();
    m_current_style = static_cast<FloatingStyle>((int32_t)cfg_style.get_value());
    
    SetWindowPos(NULL, x, y, size.cx, size.cy, SWP_NOZORDER | SWP_SHOWWINDOW);
    
    // Show window
    ShowWindow(SW_SHOW);
    update_layered_window();
    {
        CRect r;
        GetWindowRect(&r);
        FB2K_console_formatter() << "Floating Clouds: shown at " << r.left << "," << r.top
            << " " << r.Width() << "x" << r.Height()
            << " visible=" << (IsWindowVisible() ? 1 : 0);
    }
}

LRESULT FloatingCloudsWindow::OnCreate(LPCREATESTRUCT cs)
{
    return 0;
}

void FloatingCloudsWindow::OnDestroy()
{
    if (s_instance == this) s_instance = nullptr;
    stop_anim_timer();
    m_tray_icon->destroy();
    m_hotkeys->unregister_all();
    release_resources();
    SetMsgHandled(FALSE);
}

void FloatingCloudsWindow::reload_hotkeys()
{
    if (!s_instance || !s_instance->IsWindow()) return;
    s_instance->m_hotkeys->unregister_all();
    bool ok = s_instance->m_hotkeys->register_all(s_instance->m_hWnd);
    FB2K_console_formatter() << "Floating Clouds: hotkeys re-registered ok=" << (ok ? 1 : 0);
}

void FloatingCloudsWindow::apply_preferences()
{
    if (!s_instance || !s_instance->IsWindow()) return;

    // Opacity
    s_instance->update_layered_window();

    // Style: switch if changed, otherwise just repaint.
    // Migrate the saved style number (old 8-style enum -> new 6-style enum).
    cfg_var_modern::cfg_int cfg_style(cfg_guids::current_style, DEFAULT_STYLE);
    FloatingStyle st = static_cast<FloatingStyle>(migrate_style((int32_t)cfg_style.get_value()));
    if (st != s_instance->m_current_style) {
        s_instance->set_style(st);
    } else {
        s_instance->Invalidate();
    }
}

void FloatingCloudsWindow::OnPaint(CDCHandle dc)
{
    CPaintDC paint_dc(m_hWnd);
    
    if (!begin_draw()) {
        FB2K_console_formatter() << "Floating Clouds: OnPaint begin_draw FAILED";
        return;
    }
    
    // Clear with semi-transparent background
    clear_background(m_anim_opacity * 0.6f);
    
    // Delegate to style renderer
    CSize size = calculate_size();
    StyleRenderer renderer(this, this);
    renderer.render(m_current_style, size);
    
    end_draw();
}

BOOL FloatingCloudsWindow::OnEraseBkgnd(CDCHandle dc)
{
    return TRUE; // We handle everything in OnPaint
}

void FloatingCloudsWindow::OnSize(UINT nType, CSize size)
{
    if (m_render_target) {
        m_render_target->Resize(D2D1::SizeU(size.cx, size.cy));
    }
}

LRESULT FloatingCloudsWindow::OnNcHitTest(CPoint point)
{
    if (m_drag_mode) {
        return HTCAPTION; // Everything is draggable in drag mode
    }
    
    // In normal mode, check if the click is on a button
    ScreenToClient(&point);
    int btn = hit_test_button(point);
    if (btn >= 0) {
        return HTCLIENT; // Let the button handle it
    }
    
    return HTTRANSPARENT; // Everything else is transparent to clicks
}

void FloatingCloudsWindow::OnLButtonDown(UINT nFlags, CPoint point)
{
    if (m_drag_mode) {
        m_dragging = true;
        m_drag_offset = point;
        SetCapture();
        return;
    }
    
    // MD3 state feedback: pressed highlight on the button under the cursor.
    if (style_has_buttons(m_current_style)) {
        int btn = hit_test_button(point);
        if (btn >= 0) {
            m_pressed_button = btn;
            if (!m_hover_tracking) {
                m_hover_tracking = true;
                SetTimer(FC_HOVER_TIMER, 40, NULL);
            }
            Invalidate();
        }
    }
}

void FloatingCloudsWindow::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_pressed_button != -1) {
        m_pressed_button = -1;
        Invalidate();
    }
    
    if (m_dragging) {
        m_dragging = false;
        ReleaseCapture();
        
        // Save position
        CRect rect;
        GetWindowRect(&rect);
        cfg_var_modern::cfg_int cfg_x(cfg_guids::window_x, rect.left);
        cfg_var_modern::cfg_int cfg_y(cfg_guids::window_y, rect.top);
        cfg_x = rect.left;
        cfg_y = rect.top;
        return;
    }
    
    if (!m_drag_mode && m_playing) {
        // Handle button clicks
        int btn = hit_test_button(point);
        if (btn >= 0) {
            auto api = playback_control::get();
            switch (btn) {
                case 0: api->previous(); break;
                case 1: api->play_or_pause(); break;
                case 2: api->start(playback_control::track_command_next); break;
                case 3: api->volume_mute_toggle(); break;
            }
        }
    }
}

void FloatingCloudsWindow::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_dragging && m_drag_mode) {
        CPoint screen_pt = point;
        ClientToScreen(&screen_pt);
        
        CRect rect;
        GetWindowRect(&rect);
        
        int new_x = screen_pt.x - m_drag_offset.x;
        int new_y = screen_pt.y - m_drag_offset.y;
        
        SetWindowPos(NULL, new_x, new_y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        return;
    }
    
    // MD3 state feedback: the window only receives mouse messages while the
    // cursor is over a button (other areas are click-through). Start polling so
    // the hover highlight also clears when the cursor leaves the button row.
    if (style_has_buttons(m_current_style)) {
        int btn = hit_test_button(point);
        if (btn >= 0) {
            if (btn != m_hover_button) { m_hover_button = btn; Invalidate(); }
            if (!m_hover_tracking) {
                m_hover_tracking = true;
                SetTimer(FC_HOVER_TIMER, 40, NULL);
            }
        }
    }
}

LRESULT FloatingCloudsWindow::OnMouseLeave(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    if (m_hover_button != -1) { m_hover_button = -1; Invalidate(); }
    if (m_hover_tracking) { KillTimer(FC_HOVER_TIMER); m_hover_tracking = false; }
    bHandled = TRUE;
    return 0;
}

void FloatingCloudsWindow::on_hover_tick()
{
    if (!IsWindow()) return;

    CPoint sp;
    ::GetCursorPos(&sp);

    CRect rc;
    GetWindowRect(&rc);
    if (!rc.PtInRect(sp)) {
        // Cursor left the window entirely.
        if (m_hover_button != -1) { m_hover_button = -1; Invalidate(); }
        if (m_hover_tracking) { KillTimer(FC_HOVER_TIMER); m_hover_tracking = false; }
        return;
    }

    ScreenToClient(&sp);
    int btn = style_has_buttons(m_current_style) ? hit_test_button(sp) : -1;
    if (btn != m_hover_button) {
        m_hover_button = btn;
        Invalidate();
    }

    if (btn < 0 && m_pressed_button < 0) {
        // No button under the cursor and nothing pressed -> stop polling.
        if (m_hover_tracking) { KillTimer(FC_HOVER_TIMER); m_hover_tracking = false; }
    }
}

void FloatingCloudsWindow::clear_hover_state()
{
    m_hover_button = -1;
    m_pressed_button = -1;
    if (m_hover_tracking) { KillTimer(FC_HOVER_TIMER); m_hover_tracking = false; }
}

BOOL FloatingCloudsWindow::OnSetCursor(CWindow wnd, UINT nHitTest, UINT message)
{
    if (m_drag_mode) {
        SetCursor(LoadCursor(NULL, IDC_SIZEALL));
        return TRUE;
    }
    return FALSE;
}

LRESULT FloatingCloudsWindow::OnHotKey(UINT msg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    auto action = m_hotkeys->handle_hotkey(wParam);
    switch (action) {
        case HotkeyManager::ActionToggleDrag:
            toggle_drag_mode();
            break;
        case HotkeyManager::ActionToggleVisibility:
            toggle_visibility();
            break;
        case HotkeyManager::ActionCycleStyle:
            cycle_style();
            break;
    }
    return 0;
}

LRESULT FloatingCloudsWindow::OnDisplayChange(UINT msg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    // Reposition window to stay within new display bounds
    CRect rect;
    GetWindowRect(&rect);
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    
    int new_x = (std::min)((int)rect.left, (int)(screen_w - rect.Width()));
    int new_y = (std::min)((int)rect.top, (int)(screen_h - rect.Height()));
    new_x = (std::max)(0, new_x);
    new_y = (std::max)(0, new_y);
    
    SetWindowPos(NULL, new_x, new_y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    return 0;
}

LRESULT FloatingCloudsWindow::OnTrayNotify(UINT msg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if (m_tray_icon) {
        return m_tray_icon->handle_notify((UINT)lParam);
    }
    return 0;
}

void FloatingCloudsWindow::toggle_drag_mode()
{
    m_drag_mode = !m_drag_mode;
    
    if (m_drag_mode) {
        SetCursor(LoadCursor(NULL, IDC_SIZEALL));
    }
}

void FloatingCloudsWindow::toggle_visibility()
{
    m_visible = !m_visible;
    if (m_visible) {
        show_with_animation();
    } else {
        hide_with_animation();
    }
}

void FloatingCloudsWindow::show_with_animation()
{
    const bool was_hidden = !m_visible;
    m_visible = true;
    if (was_hidden) m_anim_opacity = 0.0f; // fade in from transparent
    ShowWindow(SW_SHOW);
    SetWindowPos(NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
    update_layered_window();
    start_anim_timer();
    Invalidate();
}

void FloatingCloudsWindow::hide_with_animation()
{
    m_visible = false;
    if (m_anim_opacity <= 0.001f) {
        ShowWindow(SW_HIDE);
        return;
    }
    start_anim_timer(); // fade out, then SW_HIDE when settled
}

void FloatingCloudsWindow::start_anim_timer()
{
    if (m_anim_timer == 0) {
        m_anim_timer = SetTimer(1, 16, NULL);
        m_last_anim_tick = (double)GetTickCount64();
    }
}

void FloatingCloudsWindow::stop_anim_timer()
{
    if (m_anim_timer != 0) {
        KillTimer(1);
        m_anim_timer = 0;
    }
}

LRESULT FloatingCloudsWindow::OnTimer(UINT, WPARAM wParam, LPARAM, BOOL& bHandled)
{
    if (wParam == FC_HOVER_TIMER) {
        on_hover_tick();
    } else {
        on_anim_tick();
    }
    bHandled = TRUE;
    return 0;
}

void FloatingCloudsWindow::on_anim_tick()
{
    if (!IsWindow()) { stop_anim_timer(); return; }

    m_last_anim_tick = (double)GetTickCount64();
    bool changed = false;

    // Progress: exponential smoothing toward the target -> gradual fill.
    const float k = 0.15f;
    float d = m_target_progress - m_display_progress;
    if (fabsf(d) > 0.0005f) {
        m_display_progress += d * k;
        if (fabsf(m_target_progress - m_display_progress) < 0.0005f)
            m_display_progress = m_target_progress;
        changed = true;
    } else {
        m_display_progress = m_target_progress;
    }

    // Opacity: ease toward 1 (visible) or 0 (hiding).
    const float target_op = m_visible ? 1.0f : 0.0f;
    float od = target_op - m_anim_opacity;
    if (fabsf(od) > 0.001f) {
        m_anim_opacity += od * 0.18f;
        if (fabsf(target_op - m_anim_opacity) < 0.001f) m_anim_opacity = target_op;
        changed = true;
    } else {
        m_anim_opacity = target_op;
    }

    if (changed) {
        update_layered_window();
        Invalidate();
        return; // keep the frame loop for the next frame
    }

    // Nothing is animating (progress/opacity settled).
    if (!m_visible) {
        ShowWindow(SW_HIDE); // fade-out finished
        stop_anim_timer();
        return;
    }
    // Visualizer animates continuously: keep the frame loop alive.
    if (m_current_style == FloatingStyle::Visualizer) {
        Invalidate();
        return;
    }
    if (m_marquee_active) {
        Invalidate(); // keep redrawing to advance the scrolling title
        return;       // keep the frame loop running
    }
    stop_anim_timer();
}

void FloatingCloudsWindow::cycle_style()
{
    int32_t next = (static_cast<int32_t>(m_current_style) + 1) % static_cast<int32_t>(FloatingStyle::Count);
    set_style(static_cast<FloatingStyle>(next));
}

void FloatingCloudsWindow::set_style(FloatingStyle style)
{
    m_current_style = style;
    clear_hover_state(); // stale hover/pressed must not carry across styles
    
    // Save style preference
    cfg_var_modern::cfg_int cfg_style(cfg_guids::current_style, DEFAULT_STYLE);
    cfg_style = static_cast<int32_t>(style);
    
    // Resize window
    CSize size = calculate_size();
    CRect rect;
    GetWindowRect(&rect);
    SetWindowPos(NULL, 0, 0, size.cx, size.cy, SWP_NOMOVE | SWP_NOZORDER);
    
    // Visualizer animates continuously -> make sure the frame loop is running.
    if (m_current_style == FloatingStyle::Visualizer && m_visible) {
        start_anim_timer();
    }
    
    Invalidate();
}

void FloatingCloudsWindow::on_playback_new_track(const char* title, const char* artist, 
                                                  const char* album, album_art_data_ptr art)
{
    m_title = title;
    m_artist = artist;
    m_album = album;
    m_album_art = art;
    m_album_art_dirty = true;
    m_playing = true;
    m_paused = false;
    
    // Get track length
    m_track_length = playback_control::get()->playback_get_length_ex();
    
    // Show window if auto-hidden
    if (m_visible == false) {
        show_with_animation();
    }
    
    // Resize for new content
    CSize size = calculate_size();
    SetWindowPos(NULL, 0, 0, size.cx, size.cy, SWP_NOMOVE | SWP_NOZORDER);
    
    Invalidate();
}

void FloatingCloudsWindow::on_playback_stop()
{
    m_playing = false;
    m_title.reset();
    m_artist.reset();
    m_album_art.release();
    m_album_art_bitmap = nullptr;
    m_playback_time = 0.0;
    m_track_length = 0.0;
    m_target_progress = 0.0f;
    m_lyric_lines.remove_all();
    m_plain_lines.remove_all();
    m_lyrics_has_lrc = false;
    m_lyric_index = -1;
    m_plain_index = -1;
    m_lyrics.reset();
    start_anim_timer();
    
    // Auto-hide if configured
    cfg_var_modern::cfg_bool cfg_auto_hide(cfg_guids::auto_hide, DEFAULT_AUTO_HIDE);
    if (cfg_auto_hide.get()) {
        hide_with_animation();
    }
    
    Invalidate();
}

void FloatingCloudsWindow::on_playback_time(double time)
{
    m_playback_time = time;
    m_target_progress = m_track_length > 0 ? (float)(time / m_track_length) : 0.0f;
    m_target_progress = std::clamp(m_target_progress, 0.0f, 1.0f);
    start_anim_timer(); // ease the displayed progress toward this target

    // Sync lyrics to the current time.
    if (m_lyrics_has_lrc && m_lyric_lines.get_size() > 0) {
        int idx = -1;
        for (t_size i = 0; i < m_lyric_lines.get_size(); i++) {
            if (m_lyric_lines[i].time <= time) idx = (int)i; else break;
        }
        if (idx != m_lyric_index) {
            m_lyric_index = idx;
            Invalidate();
        }
    } else if (m_plain_lines.get_size() > 0 && m_track_length > 0) {
        // Plain (no timestamps): walk lines proportionally to track progress.
        int idx = (int)(time / m_track_length * m_plain_lines.get_size());
        if (idx < 0) idx = 0;
        if (idx >= (int)m_plain_lines.get_size()) idx = (int)m_plain_lines.get_size() - 1;
        if (idx != m_plain_index) {
            m_plain_index = idx;
            Invalidate();
        }
    }
}

void FloatingCloudsWindow::on_playback_pause(bool paused)
{
    m_paused = paused;
    Invalidate();
}

void FloatingCloudsWindow::on_volume_change(float volume)
{
    m_volume = volume;
    Invalidate();
}

CSize FloatingCloudsWindow::calculate_size()
{
    // Fixed sizes per style: the window never resizes with text length.
    // Long titles scroll (marquee) inside their fixed width instead.
    switch (m_current_style) {
        case FloatingStyle::Minimal:
            return CSize(STYLE_MINIMAL_WIDTH, 32);
        
        case FloatingStyle::Full:
            return CSize(STYLE_FULL_WIDTH, STYLE_FULL_HEIGHT);
        
        case FloatingStyle::AlbumFocus:
            return CSize(320, 400);
        
        case FloatingStyle::ProgressRing:
            return CSize(200, 272);
        
        case FloatingStyle::Visualizer:
            return CSize(320, 224);
        
        case FloatingStyle::LyricsLine:
            return CSize(400, 120);
        
        default:
            return CSize(STYLE_FULL_WIDTH, STYLE_FULL_HEIGHT);
    }
}

bool FloatingCloudsWindow::get_visual_spectrum(float* bars, unsigned count)
{
    if (!bars || count == 0) return false;
    for (unsigned i = 0; i < count; i++) bars[i] = 0.0f;

    // Lazily create the real-time spectrum stream (NewFFT: normalized ~0..1).
    if (!m_vis_stream.is_valid()) {
        try {
            visualisation_manager::get()->create_stream(m_vis_stream,
                visualisation_manager::KStreamFlagNewFFT);
        } catch (...) {
            return false;
        }
    }
    if (!m_vis_stream.is_valid()) return false;

    double t = 0;
    if (!m_vis_stream->get_absolute_time(t)) return false;

    audio_chunk_impl spectrum;
    if (!m_vis_stream->get_spectrum_absolute(spectrum, t, 512)) return false;

    const audio_sample* data = spectrum.get_data();
    const t_size samples = spectrum.get_sample_count(); // == fft_size / 2
    const unsigned chans = spectrum.get_channels();
    if (!data || samples <= 0 || chans == 0) return false;

    // Map FFT bins to `count` bars (linear groups, max per group, averaged
    // across channels).
    for (unsigned b = 0; b < count; b++) {
        t_size start = (t_size)b * samples / count;
        t_size end = (t_size)(b + 1) * samples / count;
        if (end <= start) end = start + 1;
        if (end > samples) end = samples;
        double m = 0.0;
        for (t_size s = start; s < end; s++) {
            double v = 0.0;
            for (unsigned c = 0; c < chans; c++) v += data[s * chans + c];
            v /= (double)chans;
            if (v > m) m = v;
        }
        bars[b] = (float)m;
    }
    return true;
}

void FloatingCloudsWindow::on_lyrics_update(const char* text)
{
    m_lyric_lines.remove_all();
    m_plain_lines.remove_all();
    m_lyrics_has_lrc = false;
    m_lyric_index = -1;
    m_plain_index = -1;
    if (text && *text) {
        m_lyrics = text;
        parse_lyrics(text);
        FB2K_console_formatter() << "Floating Clouds: lyrics parsed lrc=" << (m_lyrics_has_lrc ? 1 : 0)
            << " lines=" << (m_lyrics_has_lrc ? m_lyric_lines.get_size() : m_plain_lines.get_size());
    } else {
        m_lyrics.reset();
    }
    Invalidate();
}

const char* FloatingCloudsWindow::get_current_lyric_line() const
{
    if (m_lyrics_has_lrc) {
        if (m_lyric_index >= 0 && m_lyric_index < (int)m_lyric_lines.get_size())
            return m_lyric_lines[m_lyric_index].text;
        return nullptr;
    }
    if (m_plain_index >= 0 && m_plain_index < (int)m_plain_lines.get_size())
        return m_plain_lines[m_plain_index];
    return nullptr;
}

namespace {
// Trim leading/trailing spaces and tabs (independent of pfc::trim's signature).
void trim_ws(pfc::string8& s)
{
    t_size len = s.length();
    t_size start = 0;
    while (start < len && (s[start] == ' ' || s[start] == '\t')) start++;
    t_size end = len;
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r')) end--;
    if (start == 0 && end == len) return;
    pfc::string8 t(s.get_ptr() + start, end - start);
    s = t;
}
}

void FloatingCloudsWindow::parse_lyrics(const char* text)
{
    if (!text) return;
    m_lyric_lines.remove_all();
    m_plain_lines.remove_all();

    const char* p = text;
    bool any_lrc = false;

    while (*p) {
        const char* nl = strchr(p, '\n');
        t_size len = nl ? (t_size)(nl - p) : (t_size)strlen(p);
        pfc::string8 line(p, len);
        p = nl ? nl + 1 : p + len;

        if (!line.is_empty() && line[line.length() - 1] == '\r')
            line.truncate(line.length() - 1);
        if (line.is_empty()) continue;

        // Collect leading [mm:ss(.xx)] timestamps (LRC).
        pfc::list_t<double> times;
        const char* s = line.get_ptr();
        while (*s == '[') {
            double t = parse_lrc_time(s);
            if (t < 0.0) break;
            times.add_item(t);
            const char* close = strchr(s, ']');
            s = close ? close + 1 : s + 1;
        }

        if (times.get_count() > 0) {
            any_lrc = true;
            pfc::string8 body(s);
            trim_ws(body);
            for (t_size i = 0; i < times.get_count(); i++) {
                LyricLine ll;
                ll.time = times[i];
                ll.text = body;
                m_lyric_lines.add_item(ll);
            }
        } else {
            trim_ws(line);
            if (!line.is_empty()) m_plain_lines.add_item(line);
        }
    }

    m_lyrics_has_lrc = any_lrc && m_lyric_lines.get_size() > 0;
    if (m_lyrics_has_lrc) {
        // Insertion sort by start time (stable for equal timestamps).
        for (t_size i = 1; i < m_lyric_lines.get_size(); i++) {
            LyricLine key = m_lyric_lines[i];
            t_size j = i;
            while (j > 0 && m_lyric_lines[j - 1].time > key.time) {
                m_lyric_lines[j] = m_lyric_lines[j - 1];
                j--;
            }
            m_lyric_lines[j] = key;
        }
    }
}

double FloatingCloudsWindow::parse_lrc_time(const char* s)
{
    if (!s || *s != '[') return -1.0;
    const char* close = strchr(s, ']');
    if (!close) return -1.0;

    const char* q = s + 1;
    if (!isdigit((unsigned char)*q)) return -1.0;
    int mm = 0;
    while (isdigit((unsigned char)*q)) { mm = mm * 10 + (*q - '0'); q++; }
    if (*q != ':') return -1.0;
    q++;
    if (!isdigit((unsigned char)*q)) return -1.0;
    int ss = 0;
    while (isdigit((unsigned char)*q)) { ss = ss * 10 + (*q - '0'); q++; }
    double frac = 0.0;
    if (*q == '.') {
        q++;
        double scale = 0.1;
        while (isdigit((unsigned char)*q)) { frac += (*q - '0') * scale; scale *= 0.1; q++; }
    }
    if (q != close) return -1.0;
    return (double)mm * 60.0 + (double)ss + frac;
}

void FloatingCloudsWindow::update_layered_window()
{
    if (!m_hWnd) return;
    // Read opacity from config so preferences changes hot-reload.
    cfg_var_modern::cfg_int cfg_opacity(cfg_guids::opacity, DEFAULT_OPACITY);
    int op = (int)cfg_opacity.get_value();
    if (op < 0 || op > 255) op = DEFAULT_OPACITY;
    ::SetLayeredWindowAttributes(m_hWnd, 0, (BYTE)(op * m_anim_opacity), LWA_ALPHA);
}

int FloatingCloudsWindow::hit_test_button(CPoint point)
{
    if (!style_has_buttons(m_current_style)) return -1;
    
    // Control button row: [<<] [Play/Pause] [>>] [Vol], bottom-centered.
    const int btn_count = 4;
    const int btn_spacing = BUTTON_SPACING;
    const int total_width = btn_count * BUTTON_SIZE + (btn_count - 1) * btn_spacing;
    
    CSize size = calculate_size();
    int start_x = button_row_start_x(size.cx);
    int btn_y = button_row_y(size.cy);
    
    for (int i = 0; i < btn_count; i++) {
        int btn_x = start_x + i * (BUTTON_SIZE + btn_spacing);
        CRect btn_rect(btn_x, btn_y, btn_x + BUTTON_SIZE, btn_y + BUTTON_SIZE);
        if (btn_rect.PtInRect(point)) {
            return i;
        }
    }
    
    return -1;
}
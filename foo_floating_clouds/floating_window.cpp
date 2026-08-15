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
    , m_playlist_panel(std::make_unique<PlaylistPanel>())
    , m_config_callback(this)
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
    
    // Initialize D2D renderer first: it picks the present mode (DirectComposition
    // on Win10/11, else the uniform-alpha Hwnd fallback).
    bool d2d_ok = D2DRenderer::initialize(m_hWnd);
    FB2K_console_formatter() << "Floating Clouds: D2D initialize=" << (d2d_ok ? 1 : 0)
        << " mode=" << (is_ulw() ? "ULW" : "Hwnd");
    
    // Both present modes use WS_EX_LAYERED (ULW presents via UpdateLayeredWindow,
    // the Hwnd fallback via LWA_ALPHA).
    DWORD ex_style = FLOATING_WINDOW_EX_STYLE | WS_EX_LAYERED;
    SetWindowLong(GWL_EXSTYLE, ex_style);
    SetWindowPos(NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    FB2K_console_formatter() << "Floating Clouds: exstyle=0x" << GetWindowLong(GWL_EXSTYLE)
        << " mode=" << (is_ulw() ? "ULW" : "Hwnd");
    
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
    // Migrate the saved style number ONCE at load (old 8-style enum -> new
    // 6-style) and persist the migrated value, so apply_preferences must never
    // re-migrate (re-migrating a new 0-5 value corrupts the style: 2->1, 3->0...).
    {
        int saved = (int)cfg_style.get_value();
        m_current_style = static_cast<FloatingStyle>(migrate_style(saved));
        if ((int)m_current_style != saved) cfg_style = (int)m_current_style;
    }
    // Apply the saved skin (0 = MD3 default, 1 = Apple).
    {
        cfg_var_modern::cfg_int cfg_skin(cfg_guids::current_skin, DEFAULT_SKIN);
        int saved = (int)cfg_skin.get_value();
        FloatingSkin sk = (saved >= 0 && saved < (int)FloatingSkin::Count)
            ? static_cast<FloatingSkin>(saved) : static_cast<FloatingSkin>(DEFAULT_SKIN);
        if ((int)sk != saved) cfg_skin = (int)sk;
        D2DRenderer::set_skin(sk);
    }
    // Apply the saved color mode (0 = follow foobar2000, 1 = dark, 2 = light).
    {
        cfg_var_modern::cfg_int cfg_mode(cfg_guids::color_mode, DEFAULT_COLOR_MODE);
        int saved = (int)cfg_mode.get_value();
        m_color_mode = (saved >= 0 && saved < (int)FloatingColorMode::Count)
            ? static_cast<FloatingColorMode>(saved) : static_cast<FloatingColorMode>(DEFAULT_COLOR_MODE);
        if ((int)m_color_mode != saved) cfg_mode = (int)m_color_mode;
        D2DRenderer::set_light(effective_light());
    }
    // Apply the font family (custom cfg, else foobar2000's default UI font).
    refresh_font_family();
    
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
    if (m_playlist_panel && m_playlist_panel->IsWindow()) m_playlist_panel->DestroyWindow();
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

    // Style: switch if changed, otherwise just repaint. The config already
    // holds the new 6-style enum (migrated once at load), so no re-migration.
    cfg_var_modern::cfg_int cfg_style(cfg_guids::current_style, DEFAULT_STYLE);
    int v = (int)cfg_style.get_value();
    FloatingStyle st = (v >= 0 && v < (int)FloatingStyle::Count)
        ? static_cast<FloatingStyle>(v) : static_cast<FloatingStyle>(DEFAULT_STYLE);
    if (st != s_instance->m_current_style) {
        s_instance->set_style(st);
    } else {
        s_instance->Invalidate();
    }

    // Skin: switch if changed (hot reload from Preferences / tray).
    cfg_var_modern::cfg_int cfg_skin(cfg_guids::current_skin, DEFAULT_SKIN);
    int sv = (int)cfg_skin.get_value();
    FloatingSkin sk = (sv >= 0 && sv < (int)FloatingSkin::Count)
        ? static_cast<FloatingSkin>(sv) : static_cast<FloatingSkin>(DEFAULT_SKIN);
    if (sk != s_instance->get_skin()) {
        s_instance->set_skin(sk);
    }

    // Color mode: switch if changed (hot reload from Preferences).
    cfg_var_modern::cfg_int cfg_mode(cfg_guids::color_mode, DEFAULT_COLOR_MODE);
    int mv = (int)cfg_mode.get_value();
    FloatingColorMode cm = (mv >= 0 && mv < (int)FloatingColorMode::Count)
        ? static_cast<FloatingColorMode>(mv) : static_cast<FloatingColorMode>(DEFAULT_COLOR_MODE);
    if (cm != s_instance->m_color_mode) {
        s_instance->set_color_mode(cm);
    }

    // Font family: re-resolve (custom cfg may have changed).
    s_instance->refresh_font_family();
}

bool FloatingCloudsWindow::render_now()
{
    // Follow foobar2000 light/dark changes between paints (m_dark_hooks
    // updates itself via ui_config callback; we just compare + apply).
    const bool light = effective_light();
    if (light != is_light()) {
        D2DRenderer::set_light(light);
        if (m_playlist_panel && m_playlist_panel->IsWindow()) {
            m_playlist_panel->set_light(light);
        }
    }

    if (!begin_draw()) {
        FB2K_console_formatter() << "Floating Clouds: render_now begin_draw FAILED";
        return false;
    }

    // Show/hide vertical drift (plan 004): translate the whole surface down
    // while hiding (and up while showing) so the fade reads as a float.
    if (m_anim_lift > 0.01f) {
        m_render_target->SetTransform(D2D1::Matrix3x2F::Translation(0.0f, m_anim_lift));
    }
    
    if (is_ulw()) {
        // Per-pixel alpha: clear transparent, then draw the rounded card + shadow.
        clear_background(0.0f);
        CRect rc;
        GetClientRect(&rc);
        D2D1_RECT_F card = D2D1::RectF((float)SHADOW_INSET, (float)SHADOW_INSET,
                                       (float)rc.Width() - (float)SHADOW_INSET,
                                       (float)rc.Height() - (float)SHADOW_INSET);
        draw_surface_card(card, skin().corner_card);
    } else {
        // Uniform-alpha fallback: opaque surface (per-pixel alpha is ignored).
        clear_background(m_anim_opacity * 0.6f);
    }
    
    // Delegate to style renderer
    CSize size = calculate_size();
    StyleRenderer renderer(this, this);
    renderer.render(m_current_style, size);
    
    end_draw();
    return true;
}

void FloatingCloudsWindow::OnPaint(CDCHandle dc)
{
    CPaintDC paint_dc(m_hWnd);
    render_now();
}

BOOL FloatingCloudsWindow::OnEraseBkgnd(CDCHandle dc)
{
    return TRUE; // We handle everything in OnPaint
}

void FloatingCloudsWindow::OnSize(UINT nType, CSize size)
{
    on_resize(size);
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
            start_anim_timer(); // ease the pressed state layer in (plan 001)
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
        start_anim_timer(); // ease the pressed state layer out (plan 001)
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
                case 4: {
                    // Playlist picker: show the D2D panel below/above the window.
                    if (m_playlist_panel) {
                        CRect r;
                        GetWindowRect(&r);
                        CPoint pos(r.left, r.bottom + 8);
                        const int sh = GetSystemMetrics(SM_CYSCREEN);
                        if (pos.y + PlaylistPanel::PANEL_HEIGHT > sh)
                            pos.y = r.top - PlaylistPanel::PANEL_HEIGHT - 8;
                        if (pos.y < 0) pos.y = 0;
                        m_playlist_panel->open(m_hWnd, pos);
                    }
                    break;
                }
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
            if (btn != m_hover_button) { m_hover_button = btn; start_anim_timer(); Invalidate(); }
            if (!m_hover_tracking) {
                m_hover_tracking = true;
                SetTimer(FC_HOVER_TIMER, 40, NULL);
            }
        }
    }
}

LRESULT FloatingCloudsWindow::OnMouseLeave(UINT, WPARAM, LPARAM, BOOL& bHandled)
{
    if (m_hover_button != -1) { m_hover_button = -1; start_anim_timer(); Invalidate(); }
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
        start_anim_timer(); // ease the hover state layer in/out (plan 001)
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
        case HotkeyManager::ActionCycleSkin:
            cycle_skin();
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
    FB2K_console_formatter() << "Floating Clouds: show (was_hidden=" << (was_hidden ? 1 : 0) << ")";
    ShowWindow(SW_SHOW);
    SetWindowPos(NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
    update_layered_window();
    start_anim_timer();
    Invalidate();
}

void FloatingCloudsWindow::hide_with_animation()
{
    m_visible = false;
    FB2K_console_formatter() << "Floating Clouds: hide (opacity=" << (int)(m_anim_opacity * 255.0f) << ")";
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

    // Time-based easing (frame-rate independent, plan 002). dt is clamped so a
    // stall (debugger, sleep, hotkey) doesn't cause a huge single-frame jump.
    const double now = (double)GetTickCount64();
    const float dt = (float)((now - m_last_anim_tick) / 1000.0);
    m_last_anim_tick = now;
    const float dtc = (dt < 0.0f) ? 0.0f : (dt > 0.10f ? 0.10f : dt);
    bool changed = false;

    // Progress: time-based ease toward the target (tau 0.10s preserves the old
    // k=0.15/frame @60fps feel).
    const float np = approach_dt(m_display_progress, m_target_progress, 0.10f, dtc);
    if (fabsf(np - m_display_progress) > 0.0005f) changed = true;
    m_display_progress = np;

    // Opacity: time-based ease toward 1 (visible) / 0 (hiding) (tau 0.085s).
    const float target_op = m_visible ? 1.0f : 0.0f;
    const float no = approach_dt(m_anim_opacity, target_op, 0.085f, dtc);
    if (fabsf(no - m_anim_opacity) > 0.001f) changed = true;
    m_anim_opacity = no;

    // Show/hide vertical drift (plan 004): 0 when visible, SHADOW_INSET when hiding.
    const float lift_tgt = m_visible ? 0.0f : (float)SHADOW_INSET;
    const float nl = approach_dt(m_anim_lift, lift_tgt, 0.12f, dtc);
    if (fabsf(nl - m_anim_lift) > 0.01f) changed = true;
    m_anim_lift = nl;

    // Button state layers (plan 001): ease each toward its target.
    for (int i = 0; i < BUTTON_COUNT; i++) {
        const float tgt = (m_pressed_button == i) ? skin().pressed_state
                        : (m_hover_button == i)  ? skin().hover_state : 0.0f;
        const float nv = approach_dt(m_button_state_layer[i], tgt, 0.08f, dtc);
        if (fabsf(nv - m_button_state_layer[i]) > 0.0005f) changed = true;
        m_button_state_layer[i] = nv;
    }

    if (changed) {
        update_layered_window(); // applies opacity (ULW) / LWA (fallback)
        render_now();            // render immediately (frame-loop paced, not WM_PAINT)
        return; // keep the frame loop for the next frame
    }

    // Nothing is animating (progress/opacity settled).
    if (!m_visible) {
        ShowWindow(SW_HIDE); // fade-out finished
        FB2K_console_formatter() << "Floating Clouds: SW_HIDE after fade-out";
        stop_anim_timer();
        return;
    }
    // Visualizer animates continuously: keep the frame loop alive.
    if (m_current_style == FloatingStyle::Visualizer) {
        render_now();
        return;
    }
    if (m_marquee_active) {
        render_now(); // keep redrawing to advance the scrolling title
        return;       // keep the frame loop running
    }
    // During playback keep repainting continuously: the playback-time callback
    // only fires ~13/s, so content would otherwise update in short bursts and
    // then freeze — on the per-pixel-alpha ULW surface that low, uneven
    // refresh reads as flicker/strobe. (Visualizer does the same above.)
    if (m_playing && !m_paused) {
        render_now();
        return; // keep the frame loop running while playing
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

void FloatingCloudsWindow::cycle_skin()
{
    int32_t next = (static_cast<int32_t>(get_skin()) + 1) % static_cast<int32_t>(FloatingSkin::Count);
    set_skin(static_cast<FloatingSkin>(next));
}

void FloatingCloudsWindow::set_skin(FloatingSkin skin)
{
    // Rebuild per-skin renderer resources (shadow, specular brush, fonts).
    D2DRenderer::set_skin(skin);

    // Save skin preference
    cfg_var_modern::cfg_int cfg_skin(cfg_guids::current_skin, DEFAULT_SKIN);
    cfg_skin = static_cast<int32_t>(skin);

    // Keep the playlist panel on the same skin + light/dark mode + font.
    if (m_playlist_panel && m_playlist_panel->IsWindow()) {
        m_playlist_panel->set_skin(skin);
        m_playlist_panel->set_light(effective_light());
        m_playlist_panel->set_font_family(get_font_family());
    }

    Invalidate();
}

void FloatingCloudsWindow::set_color_mode(FloatingColorMode mode)
{
    m_color_mode = mode;

    // Save preference
    cfg_var_modern::cfg_int cfg_mode(cfg_guids::color_mode, DEFAULT_COLOR_MODE);
    cfg_mode = static_cast<int32_t>(mode);

    // Apply the effective light/dark token set everywhere.
    D2DRenderer::set_light(effective_light());
    if (m_playlist_panel && m_playlist_panel->IsWindow()) {
        m_playlist_panel->set_light(effective_light());
        m_playlist_panel->set_font_family(get_font_family());
    }

    Invalidate();
}

bool FloatingCloudsWindow::effective_light() const
{
    switch (m_color_mode) {
        case FloatingColorMode::Light: return true;
        case FloatingColorMode::Dark:  return false;
        case FloatingColorMode::Follow:
        default:
            // m_dark_hooks auto-updates from foobar2000's ui_config callback.
            return !m_dark_hooks.IsDark();
    }
}

void FloatingCloudsWindow::ConfigCallback::ui_fonts_changed()
{
    if (m_w) m_w->refresh_font_family();
}

void FloatingCloudsWindow::refresh_font_family()
{
    std::wstring family = L"Segoe UI";

    // 1) Explicit custom font from Preferences (empty = follow foobar2000).
    cfg_var_modern::cfg_string cfg_font(cfg_guids::font_family, "");
    const char* custom = cfg_font.get();
    if (custom && *custom) {
        pfc::stringcvt::string_wide_from_utf8 wide(custom);
        if (!wide.is_empty()) family = wide.get_ptr();
    } else {
        // 2) Follow foobar2000's default UI font.
        auto api = ui_config_manager::tryGet();
        if (api.is_valid()) {
            t_ui_font font = api->query_font(ui_font_default);
            if (font) {
                LOGFONTW lf = {};
                if (::GetObjectW((HFONT)font, sizeof(lf), &lf) && lf.lfFaceName[0]) {
                    family = lf.lfFaceName;
                }
            }
        }
    }

    if (family != get_font_family()) {
        D2DRenderer::set_font_family(family.c_str());
        if (m_playlist_panel && m_playlist_panel->IsWindow()) {
            m_playlist_panel->set_font_family(family.c_str());
        }
        Invalidate();
    }
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
    FB2K_console_formatter() << "Floating Clouds: new track (visible=" << (m_visible ? 1 : 0) << ")";
    
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
    FB2K_console_formatter() << "Floating Clouds: playback stop (auto_hide=" << (cfg_auto_hide.get() ? 1 : 0) << ")";
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
            return CSize(STYLE_MINIMAL_WIDTH, STYLE_MINIMAL_HEIGHT);
        
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

    // 2048-point FFT -> 1024 bins; ~21 Hz/bin at 44.1 kHz gives real bass
    // resolution instead of one bar per 16 linear bins.
    const unsigned fft_size = 2048;
    audio_chunk_impl spectrum;
    if (!m_vis_stream->get_spectrum_absolute(spectrum, t, fft_size)) {
        // Real data may not be ready right after stream creation; the SDK
        // provides a fake spectrum for exactly this case.
        try {
            m_vis_stream->make_fake_spectrum_absolute(spectrum, t, fft_size);
        } catch (...) {
            return false;
        }
    }

    const audio_sample* data = spectrum.get_data();
    const t_size bins = spectrum.get_sample_count(); // == fft_size / 2
    const unsigned chans = spectrum.get_channels();
    const unsigned srate = spectrum.get_sample_rate();
    if (!data || bins < 2 || chans == 0 || srate == 0) return false;

    // Musical spectrum: log-spaced bars from 40 Hz to 16 kHz (or 90% Nyquist).
    // Linear bin grouping makes bass look dead and treble look noisy.
    const float nyquist = (float)srate * 0.5f;
    const float min_freq = 40.0f;
    const float max_freq = (std::min)(16000.0f, nyquist * 0.9f);

    for (unsigned b = 0; b < count; b++) {
        const float t0 = (float)b / (float)count;
        const float t1 = (float)(b + 1) / (float)count;
        const float f0 = min_freq * powf(max_freq / min_freq, t0);
        const float f1 = min_freq * powf(max_freq / min_freq, t1);

        t_size bin0 = (t_size)floorf(f0 * (float)fft_size / (float)srate);
        t_size bin1 = (t_size)ceilf(f1 * (float)fft_size / (float)srate);
        if (bin0 < 1) bin0 = 1; // skip DC bin
        if (bin1 <= bin0) bin1 = bin0 + 1;
        if (bin1 > bins) bin1 = bins;

        // Average magnitude over the band (channels averaged first). Averaging
        // is stable; per-bin max over-reports noise spikes.
        double sum = 0.0;
        for (t_size s = bin0; s < bin1; s++) {
            double v = 0.0;
            for (unsigned c = 0; c < chans; c++) v += data[s * chans + c];
            v /= (double)chans;
            sum += v;
        }
        const double mag = sum / (double)(bin1 - bin0);

        // Magnitude is linear; hearing is logarithmic. Scale in dB with a
        // -60 dB noise floor so quiet passages don't read as full-scale.
        const double eps = 1e-6;
        const double db = 20.0 * log10(mag + eps);
        double norm = (db + 60.0) / 60.0;
        if (norm < 0.0) norm = 0.0;
        else if (norm > 1.0) norm = 1.0;
        bars[b] = (float)norm;
    }
    return true;
}

void FloatingCloudsWindow::smooth_spectrum(const float* raw, float* out, unsigned count)
{
    const double now = (double)GetTickCount64();
    const float dt = (float)((now - m_last_vis_tick) / 1000.0);
    m_last_vis_tick = now;
    const float dtc = (dt < 0.0f) ? 0.0f : (dt > 0.25f ? 0.25f : dt);
    // Asymmetric one-pole: fast attack so kick/snare transients hit the bar
    // height the same frame, slower decay so the bar falls naturally. A
    // symmetric time constant was making bars lag behind the beat.
    const float k_up = 1.0f - expf(-dtc / 0.025f);  // ~25 ms attack
    const float k_down = 1.0f - expf(-dtc / 0.120f); // ~120 ms decay
    for (unsigned i = 0; i < count && i < 32; i++) {
        float v = raw[i];
        if (v < 0.0f) v = 0.0f;
        else if (v > 1.0f) v = 1.0f;
        const float k = (v > m_vis_smooth[i]) ? k_up : k_down;
        m_vis_smooth[i] += (v - m_vis_smooth[i]) * k;
        out[i] = m_vis_smooth[i];
    }
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

    if (is_ulw()) {
        // Per-pixel alpha: opacity/fade folded into the DIB at present time;
        // callers repaint themselves (render_now / Invalidate).
        set_global_opacity((float)op / 255.0f * m_anim_opacity);
    } else {
        // Uniform-alpha fallback.
        ::SetLayeredWindowAttributes(m_hWnd, 0, (BYTE)(op * m_anim_opacity), LWA_ALPHA);
    }
}

int FloatingCloudsWindow::hit_test_button(CPoint point)
{
    if (!style_has_buttons(m_current_style)) return -1;
    
    // Control button row: [<<] [Play/Pause] [>>] [Vol] [Playlist], bottom-centered.
    const int btn_count = button_row_count();
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
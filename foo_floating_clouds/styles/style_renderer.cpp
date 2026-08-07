#include "stdafx.h"
#include "style_renderer.h"
#include "../config.h"

// ============================================================================
// StyleRenderer implementation
// ============================================================================

StyleRenderer::StyleRenderer(FloatingCloudsWindow* window, D2DRenderer* renderer)
    : m_window(window), m_renderer(renderer)
{
}

StyleRenderer::~StyleRenderer()
{
}

void StyleRenderer::render(FloatingStyle style, const CSize& window_size)
{
    switch (style) {
        case FloatingStyle::Minimal:        render_minimal(window_size); break;
        case FloatingStyle::Full:           render_full(window_size); break;
        case FloatingStyle::AlbumFocus:     render_album_focus(window_size); break;
        case FloatingStyle::ProgressRing:   render_progress_ring(window_size); break;
        case FloatingStyle::Visualizer:     render_visualizer(window_size); break;
        case FloatingStyle::LyricsLine:     render_lyrics_line(window_size); break;
        default:                            render_full(window_size); break;
    }
}

void StyleRenderer::render_full(const CSize& size)
{
    float pad = (float)WINDOW_PADDING;
    float art_size = (float)COVER_ART_SIZE_FULL;
    float avail_w = (float)size.cx - pad * 3 - art_size;
    float y = pad;
    
    m_renderer->draw_album_art(pad, pad, art_size, m_window->get_album_art());
    
    float text_x = pad + art_size + pad;
    
    pfc::stringcvt::string_wide_from_utf8 wtitle(m_window->get_title());
    m_renderer->draw_text(wtitle, text_x, y, avail_w, 20,
                          m_renderer->get_title_format(),
                          D2DRenderer::hex(md3::on_surface, 0.95f));
    y += 22;
    
    pfc::stringcvt::string_wide_from_utf8 wartist(m_window->get_artist());
    m_renderer->draw_text(wartist, text_x, y, avail_w, 16,
                          m_renderer->get_artist_format(),
                          D2DRenderer::hex(md3::on_surface_variant, 0.85f));
    y += 20;
    
    float progress = m_window->get_display_progress();
    float pb_y = (float)progress_above_buttons_y(size.cy);
    m_renderer->draw_progress_bar(text_x, pb_y, avail_w, (float)PROGRESS_BAR_HEIGHT, progress,
                                  D2DRenderer::hex(md3::primary, 0.9f),
                                  D2DRenderer::hex(md3::on_surface_variant, 0.25f));
    
    // Control buttons: prev, play/pause, next, mute
    draw_button_row(size);
}

void StyleRenderer::draw_button_row(const CSize& size)
{
    const int btn_count = BUTTON_COUNT;
    const int btn_spacing = BUTTON_SPACING;
    const int total_width = btn_count * BUTTON_SIZE + (btn_count - 1) * btn_spacing;
    float btn_start_x = ((float)size.cx - total_width) / 2;
    float btn_y = (float)button_row_y(size.cy);

    // prev, play/pause, next, mute, playlist picker
    const wchar_t* icons[btn_count] = { L"\u23EE", L"\u23EF", L"\u23ED", L"\U0001F507", L"\u2630" };
    bool active[btn_count] = { false, m_window->is_paused(), false, m_window->is_volume_muted(), false };

    for (int i = 0; i < btn_count; i++) {
        float btn_x = btn_start_x + i * (BUTTON_SIZE + btn_spacing);

        // MD3 state: pressed > hover > normal.
        int state = (m_window->get_pressed_button() == i) ? 2
                  : (m_window->get_hover_button() == i)  ? 1 : 0;

        // Icon color: playing play/pause -> primary; muted volume -> error.
        D2D1_COLOR_F icon_color = D2DRenderer::hex(md3::on_surface_variant, 0.95f);
        if (i == 1 && m_window->is_playing() && !m_window->is_paused()) {
            icon_color = D2DRenderer::hex(md3::primary, 1.0f);
        } else if (i == 3 && m_window->is_volume_muted()) {
            icon_color = D2DRenderer::hex(md3::error, 1.0f);
        }

        m_renderer->draw_button(btn_x, btn_y, (float)BUTTON_SIZE, icons[i], active[i],
                                icon_color, state);
    }
}

void StyleRenderer::render_minimal(const CSize& size)
{
    float pad = 8;
    float avail_w = (float)size.cx - pad * 2;
    
    pfc::string8 display;
    if (m_window->is_playing()) {
        display << (m_window->is_paused() ? "\xE2\x9A\x90 " : "\xE2\x96\xB6 ");
    }
    display << m_window->get_title() << "  \xC2\xB7  " << m_window->get_artist();
    
    pfc::stringcvt::string_wide_from_utf8 wdisplay(display);
    m_renderer->draw_text(wdisplay, pad, 0, avail_w, (float)size.cy,
                          m_renderer->get_title_format(),
                          D2DRenderer::hex(md3::on_surface, 0.95f));
    
    float progress = m_window->get_display_progress();
    m_renderer->draw_progress_bar(pad, (float)size.cy - 2, avail_w, 2, progress,
                                  D2DRenderer::hex(md3::primary, 0.9f),
                                  D2DRenderer::hex(md3::on_surface_variant, 0.25f));
}

void StyleRenderer::render_album_focus(const CSize& size)
{
    float pad = (float)WINDOW_PADDING;
    float avail_w = (float)size.cx - pad * 2;
    
    float art_size = (std::min)(avail_w - pad * 2, (float)size.cy - 120);
    float art_x = ((float)size.cx - art_size) / 2;
    float art_y = pad;
    
    m_renderer->draw_album_art(art_x, art_y, art_size, m_window->get_album_art());
    
    float info_y = art_y + art_size + pad;
    
    pfc::stringcvt::string_wide_from_utf8 wtitle(m_window->get_title());
    m_renderer->draw_text(wtitle, pad, info_y, avail_w, 22,
                          m_renderer->get_title_format(),
                          D2DRenderer::hex(md3::on_surface, 0.95f));
    
    float info_y2 = info_y + 24;
    pfc::stringcvt::string_wide_from_utf8 wartist(m_window->get_artist());
    m_renderer->draw_text(wartist, pad, info_y2, avail_w, 18,
                          m_renderer->get_artist_format(),
                          D2DRenderer::hex(md3::on_surface_variant, 0.85f));
    
    float progress = m_window->get_display_progress();
    float pb_y = (float)progress_above_buttons_y(size.cy);
    m_renderer->draw_progress_bar(art_x, pb_y, art_size, (float)PROGRESS_BAR_HEIGHT, progress,
                                  D2DRenderer::hex(md3::primary, 0.9f),
                                  D2DRenderer::hex(md3::on_surface_variant, 0.25f));

    draw_button_row(size);
}

void StyleRenderer::render_progress_ring(const CSize& size)
{
    float cx = (float)size.cx / 2;
    float cy = (float)size.cy / 2 - 20;
    float ring_radius = 60.0f;
    float ring_thickness = 6.0f;
    float art_size = 80.0f;
    
    float progress = m_window->get_display_progress();
    
    m_renderer->draw_progress_ring(cx, cy, ring_radius, ring_thickness, progress,
                                   D2DRenderer::hex(md3::primary, 0.9f),
                                   D2DRenderer::hex(md3::on_surface_variant, 0.25f));
    
    m_renderer->draw_album_art(cx - art_size/2, cy - art_size/2, art_size, m_window->get_album_art());
    
    float text_y = cy + ring_radius + 16;
    float avail_w = (float)size.cx - WINDOW_PADDING * 2;
    
    pfc::stringcvt::string_wide_from_utf8 wtitle(m_window->get_title());
    m_renderer->draw_text(wtitle, (float)WINDOW_PADDING, text_y, avail_w, 18,
                          m_renderer->get_title_format(),
                          D2DRenderer::hex(md3::on_surface, 0.95f));
    
    pfc::stringcvt::string_wide_from_utf8 wartist(m_window->get_artist());
    m_renderer->draw_text(wartist, (float)WINDOW_PADDING, text_y + 20, avail_w, 16,
                          m_renderer->get_artist_format(),
                          D2DRenderer::hex(md3::on_surface_variant, 0.85f));

    draw_button_row(size);
}

void StyleRenderer::render_visualizer(const CSize& size)
{
    float pad = (float)WINDOW_PADDING;
    float avail_w = (float)size.cx - pad * 2;
    // Bars fill the space above the caption; the caption sits above the button row.
    float bar_area_h = (float)button_row_y(size.cy) - 8.0f - 16.0f - 8.0f;
    const int bar_count = 32;
    float bar_w = (avail_w - (bar_count - 1)) / bar_count;

    // Real-time FFT spectrum (0..1 per bar); bars stay flat when playback is
    // stopped or no spectrum is available yet.
    float bars[bar_count] = {};
    m_window->get_visual_spectrum(bars, bar_count);

    for (int i = 0; i < bar_count; i++) {
        float v = bars[i] * 1.8f; // boost for a livelier look
        if (v < 0.0f) v = 0.0f;
        else if (v > 1.0f) v = 1.0f;
        float h = v * bar_area_h;
        float x = pad + i * (bar_w + 1);
        float y = bar_area_h - h;

        float intensity = (float)i / bar_count;
        // MD3 accent ramp: primary (lavender) -> tertiary (pink)
        const uint32_t c1 = md3::primary;
        const uint32_t c2 = md3::tertiary;
        float r = (float)((c1 >> 16) & 0xFF) * (1.0f - intensity) + (float)((c2 >> 16) & 0xFF) * intensity;
        float g = (float)((c1 >> 8)  & 0xFF) * (1.0f - intensity) + (float)((c2 >> 8)  & 0xFF) * intensity;
        float b = (float)((c1)       & 0xFF) * (1.0f - intensity) + (float)((c2)       & 0xFF) * intensity;
        m_renderer->get_brush()->SetColor(D2D1::ColorF(r / 255.0f, g / 255.0f, b / 255.0f, 0.7f));
        m_renderer->get_render_target()->FillRectangle(
            D2D1::RectF(x, y, x + bar_w, bar_area_h), m_renderer->get_brush());
    }
    
    float text_y = bar_area_h + 8;
    pfc::string8 display;
    display << m_window->get_title() << "  \xC2\xB7  " << m_window->get_artist();
    pfc::stringcvt::string_wide_from_utf8 wdisplay(display);
    m_renderer->draw_text(wdisplay, pad, text_y, avail_w, 16,
                          m_renderer->get_artist_format(),
                          D2DRenderer::hex(md3::on_surface_variant, 0.85f));

    draw_button_row(size);
}

void StyleRenderer::render_lyrics_line(const CSize& size)
{
    float pad = (float)WINDOW_PADDING;
    float avail_w = (float)size.cx - pad * 2;
    
    pfc::stringcvt::string_wide_from_utf8 wtitle(m_window->get_title());
    m_renderer->draw_text(wtitle, pad, pad, avail_w, 18,
                          m_renderer->get_title_format(),
                          D2DRenderer::hex(md3::on_surface_variant, 0.7f));
    
    pfc::stringcvt::string_wide_from_utf8 wartist(m_window->get_artist());
    m_renderer->draw_text(wartist, pad, pad + 20, avail_w, 14,
                          m_renderer->get_artist_format(),
                          D2DRenderer::hex(md3::on_surface_variant, 0.5f));
    
    float lyrics_y = (float)size.cy / 2 - 12;
    const char* lyric = m_window->get_current_lyric_line();
    if (lyric && *lyric) {
        pfc::stringcvt::string_wide_from_utf8 wlyric(lyric);
        m_renderer->draw_text(wlyric, pad, lyrics_y, avail_w, 24,
                              m_renderer->get_title_format(),
                              D2DRenderer::hex(md3::on_surface, 0.9f));
    } else {
        m_renderer->draw_text(L"\u266B  Now Playing  \u266B", pad, lyrics_y, avail_w, 24,
                              m_renderer->get_title_format(),
                              D2DRenderer::hex(md3::on_surface, 0.9f));
    }
    
    float progress = m_window->get_display_progress();
    m_renderer->draw_progress_bar(pad, (float)size.cy - pad - PROGRESS_BAR_HEIGHT,
                                   avail_w, (float)PROGRESS_BAR_HEIGHT, progress,
                                   D2DRenderer::hex(md3::primary, 0.9f),
                                   D2DRenderer::hex(md3::on_surface_variant, 0.25f));
}
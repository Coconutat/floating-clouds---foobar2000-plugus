#pragma once

#include "stdafx.h"
#include "config.h"
#include "floating_window.h"

// ============================================================================
// StyleRenderer - Renders each FloatingStyle variant
// ============================================================================

class StyleRenderer
{
public:
    StyleRenderer(FloatingCloudsWindow* window, D2DRenderer* renderer);
    ~StyleRenderer();

    // Render the current style
    void render(FloatingStyle style, const CSize& window_size);

private:
    void render_full(const CSize& size);
    void render_minimal(const CSize& size);
    void render_album_focus(const CSize& size);
    void render_progress_ring(const CSize& size);
    void render_visualizer(const CSize& size);
    void render_visualizer_art(const CSize& size);
    void render_lyrics_line(const CSize& size);
    void draw_button_row(const CSize& size);
    // Shared by Visualizer and VisualizerArt: 32 FFT bars in [x, x+width)
    // whose baseline is bar_area_bottom and whose tallest bar is bar_area_top.
    void draw_spectrum_bars(float x, float width, float bar_area_top, float bar_area_bottom);

    FloatingCloudsWindow* m_window;
    D2DRenderer* m_renderer;
};
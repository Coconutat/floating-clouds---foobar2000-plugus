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

    // Calculate required size for a style
    static CSize calculate_size(FloatingStyle style, const pfc::string8& title, 
                                 const pfc::string8& artist);

private:
    void render_mini(const CSize& size);
    void render_mini_art(const CSize& size);
    void render_full(const CSize& size);
    void render_minimal_line(const CSize& size);
    void render_album_focus(const CSize& size);
    void render_progress_ring(const CSize& size);
    void render_visualizer(const CSize& size);
    void render_lyrics_line(const CSize& size);

    FloatingCloudsWindow* m_window;
    D2DRenderer* m_renderer;
};
#pragma once

#include "stdafx.h"

// ============================================================================
// D2DRenderer - Direct2D rendering engine for the floating window
// ============================================================================

class D2DRenderer
{
public:
    D2DRenderer();
    virtual ~D2DRenderer();

    // Initialize D2D resources (picks the present mode: DirectComposition when
    // available, else the uniform-alpha HwndRenderTarget fallback).
    bool initialize(HWND hwnd);
    
    // Release and recreate resources (on resize, DPI change, device reset, etc.)
    void release_resources();
    bool create_resources();
    
    // Begin/end a render pass
    bool begin_draw();
    void end_draw();
    
    // Clear background. In ULW mode clears to fully transparent (the surface
    // card + shadow come from draw_surface_card); in the Hwnd fallback it paints
    // the opaque surface color (per-pixel alpha is ignored there).
    void clear_background(float opacity = 0.6f);

    // Present mode: UpdateLayeredWindow (per-pixel alpha, driver-agnostic — AMD
    // DirectComposition per-pixel surfaces are known to flicker) or the
    // HwndRenderTarget + LWA_ALPHA uniform-alpha fallback.
    enum class PresentMode { None, ULW, Hwnd };

    // Present mode
    bool is_ulw() const { return m_mode == PresentMode::ULW; }
    PresentMode get_present_mode() const { return m_mode; }

    // Global window opacity (ULW: folded into per-pixel alpha at present time).
    void set_global_opacity(float opacity);

    // Rounded surface card + elevation shadow (ULW) / opaque fill (fallback).
    void draw_surface_card(const D2D1_RECT_F& rect, float radius);

    // Recreate the present target for a new client size.
    void on_resize(CSize size);
    
    // Draw rounded rectangle background
    void draw_rounded_rect(const D2D1_RECT_F& rect, float radius, 
                           const D2D1_COLOR_F& color, float opacity = 1.0f);
    
    // Draw text (long text auto-scrolls as a marquee when it overflows the box)
    void draw_text(const wchar_t* text, float x, float y, float width, float height,
                   IDWriteTextFormat* format, const D2D1_COLOR_F& color);

    // True if the last rendered frame contained any scrolling (marquee) text
    bool is_marquee_active() const { return m_marquee_active; }
    
    // Draw progress bar
    void draw_progress_bar(float x, float y, float width, float height,
                           float progress, const D2D1_COLOR_F& fg_color, 
                           const D2D1_COLOR_F& bg_color);
    
    // Draw album art
    void draw_album_art(float x, float y, float size, album_art_data_ptr art);
    
    // Draw a control button (circle with icon). state: 0 normal, 1 hover, 2 pressed.
    // state_alpha is the eased MD3 state-layer opacity (0..pressed_state) that
    // drives the hover/pressed transition and the press-scale (plan 001).
    void draw_button(float x, float y, float size, const wchar_t* icon_text,
                     bool is_active, const D2D1_COLOR_F& color, int state = 0, float state_alpha = 0.0f);
    
    // Draw a rounded progress ring
    void draw_progress_ring(float cx, float cy, float radius, float thickness,
                            float progress, const D2D1_COLOR_F& fg_color,
                            const D2D1_COLOR_F& bg_color);

    // Measure the natural single-line width of text in the given format
    float measure_text_width(const wchar_t* text, IDWriteTextFormat* format);
    
    // Getters
    ID2D1RenderTarget* get_render_target() const { return m_render_target; }
    ID2D1SolidColorBrush* get_brush() const { return m_brush; }
    IDWriteFactory* get_dwrite_factory() const { return m_dwrite_factory; }
    IWICImagingFactory* get_wic_factory() const { return m_wic_factory; }
    
    // Color helpers
    static D2D1_COLOR_F rgba(float r, float g, float b, float a = 1.0f) {
        return D2D1::ColorF(r, g, b, a);
    }
    
    static D2D1_COLOR_F hex(uint32_t hex, float a = 1.0f) {
        return D2D1::ColorF(hex, a);
    }

    // Font helpers
    IDWriteTextFormat* get_text_format(float size, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL);
    IDWriteTextFormat* get_title_format();
    IDWriteTextFormat* get_artist_format();
    IDWriteTextFormat* get_small_format();

protected:
    HWND m_hwnd = nullptr;

    PresentMode m_mode = PresentMode::None;

    // D2D / present targets
    ID2D1Factory1* m_d2d_factory = nullptr;
    ID2D1RenderTarget* m_render_target = nullptr;   // active drawing target
    ID2D1HwndRenderTarget* m_hwnd_target = nullptr; // Hwnd fallback target
    ID2D1DCRenderTarget* m_ulw_target = nullptr;    // ULW target over the DIB

    // ULW per-pixel-alpha path (DIB + UpdateLayeredWindow)
    HDC m_mem_dc = nullptr;
    HBITMAP m_dib = nullptr;
    void* m_dib_bits = nullptr;
    SIZE m_dib_size{};
    RECT m_dib_rect{};
    float m_global_opacity = 1.0f;

    // Surface-card shadow (ULW): CPU box-blurred rounded-rect bitmap
    ID2D1Bitmap* m_shadow_bitmap = nullptr;

    ID2D1SolidColorBrush* m_brush = nullptr;
    
    // DirectWrite
    IDWriteFactory* m_dwrite_factory = nullptr;
    IDWriteTextFormat* m_title_format = nullptr;
    IDWriteTextFormat* m_artist_format = nullptr;
    IDWriteTextFormat* m_small_format = nullptr;
    
    // WIC for album art decoding
    IWICImagingFactory* m_wic_factory = nullptr;
    
    // Album art bitmap
    ID2D1Bitmap* m_album_art_bitmap = nullptr;
    bool m_album_art_dirty = true;
    int m_album_art_display_size = 0; // display size the cached bitmap was scaled to
    
    // Rounded rect geometry
    ID2D1RoundedRectangleGeometry* m_rounded_rect_geo = nullptr;

    // Round-cap stroke style (for progress ring / glass edges)
    ID2D1StrokeStyle* m_round_stroke = nullptr;

    // Set when the last frame scrolled any long text; keeps the frame loop alive
    bool m_marquee_active = false;

    // Internal: build the ULW (per-pixel alpha) or Hwnd (fallback) stack.
    bool create_ulw_resources();
    bool create_hwnd_resources();
    bool create_brush_and_stroke();
    bool create_shadow();

    // Present-loop diagnostics (debug logging): 1s summary of frame/failure counts.
    bool debug_enabled();
    void log_present_summary();
    unsigned m_frames = 0;
    unsigned m_fail_begin = 0;
    unsigned m_fail_commit = 0;
    unsigned m_fail_surface = 0;
    unsigned m_rebuilds = 0;
    double m_report_t = 0.0;

    // Internal: horizontally scroll `text` inside the box (used by draw_text)
    void draw_text_marquee(const wchar_t* text, float x, float y, float width, float height,
                           IDWriteTextFormat* format, const D2D1_COLOR_F& color);
};
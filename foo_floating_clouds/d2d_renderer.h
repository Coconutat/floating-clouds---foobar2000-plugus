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

    // Initialize D2D resources
    bool initialize(HWND hwnd);
    
    // Release and recreate resources (on resize, DPI change, etc.)
    void release_resources();
    bool create_resources();
    
    // Begin/end a render pass
    bool begin_draw();
    void end_draw();
    
    // Clear with a semi-transparent dark background
    void clear_background(float opacity = 0.6f);
    
    // Draw rounded rectangle background
    void draw_rounded_rect(const D2D1_RECT_F& rect, float radius, 
                           const D2D1_COLOR_F& color, float opacity = 1.0f);
    
    // Draw text
    void draw_text(const wchar_t* text, float x, float y, float width, float height,
                   IDWriteTextFormat* format, const D2D1_COLOR_F& color);
    
    // Draw progress bar
    void draw_progress_bar(float x, float y, float width, float height,
                           float progress, const D2D1_COLOR_F& fg_color, 
                           const D2D1_COLOR_F& bg_color);
    
    // Draw album art
    void draw_album_art(float x, float y, float size, album_art_data_ptr art);
    
    // Draw a control button (circle with icon)
    void draw_button(float x, float y, float size, const wchar_t* icon_text,
                     bool is_active, const D2D1_COLOR_F& color);
    
    // Draw a rounded progress ring
    void draw_progress_ring(float cx, float cy, float radius, float thickness,
                            float progress, const D2D1_COLOR_F& fg_color,
                            const D2D1_COLOR_F& bg_color);
    
    // Getters
    ID2D1HwndRenderTarget* get_render_target() const { return m_render_target; }
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
    
    // Direct2D
    ID2D1Factory* m_d2d_factory = nullptr;
    ID2D1HwndRenderTarget* m_render_target = nullptr;
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
    
    // Rounded rect geometry
    ID2D1RoundedRectangleGeometry* m_rounded_rect_geo = nullptr;
};
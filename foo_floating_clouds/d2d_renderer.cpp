#include "stdafx.h"
#include "d2d_renderer.h"

// ============================================================================
// D2DRenderer implementation
// ============================================================================

D2DRenderer::D2DRenderer()
{
}

D2DRenderer::~D2DRenderer()
{
    release_resources();
    
    if (m_d2d_factory) m_d2d_factory->Release();
    if (m_dwrite_factory) m_dwrite_factory->Release();
    if (m_wic_factory) m_wic_factory->Release();
}

bool D2DRenderer::initialize(HWND hwnd)
{
    m_hwnd = hwnd;
    
    HRESULT hr;
    
    // Create D2D factory
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_d2d_factory);
    if (FAILED(hr)) { FB2K_console_formatter() << "Floating Clouds: D2D factory FAILED hr=0x" << pfc::format_hex(hr); return false; }
    
    // Create DWrite factory
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), 
                              reinterpret_cast<IUnknown**>(&m_dwrite_factory));
    if (FAILED(hr)) { FB2K_console_formatter() << "Floating Clouds: DWrite factory FAILED hr=0x" << pfc::format_hex(hr); return false; }
    
    // Create WIC factory
    hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                           IID_IWICImagingFactory, (void**)&m_wic_factory);
    if (FAILED(hr)) { FB2K_console_formatter() << "Floating Clouds: WIC factory FAILED hr=0x" << pfc::format_hex(hr); return false; }
    
    // Create render target
    bool ok = create_resources();
    if (!ok) console::print("Floating Clouds: render target creation FAILED");
    return ok;
}

void D2DRenderer::release_resources()
{
    if (m_render_target) { m_render_target->Release(); m_render_target = nullptr; }
    if (m_brush) { m_brush->Release(); m_brush = nullptr; }
    if (m_title_format) { m_title_format->Release(); m_title_format = nullptr; }
    if (m_artist_format) { m_artist_format->Release(); m_artist_format = nullptr; }
    if (m_small_format) { m_small_format->Release(); m_small_format = nullptr; }
    if (m_rounded_rect_geo) { m_rounded_rect_geo->Release(); m_rounded_rect_geo = nullptr; }
    if (m_round_stroke) { m_round_stroke->Release(); m_round_stroke = nullptr; }
    if (m_album_art_bitmap) { m_album_art_bitmap->Release(); m_album_art_bitmap = nullptr; }
}

bool D2DRenderer::create_resources()
{
    if (!m_d2d_factory) return false;
    if (m_render_target) return true;
    
    CRect rc;
    ::GetClientRect(m_hwnd, &rc);
    
    HRESULT hr = m_d2d_factory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
        ),
        D2D1::HwndRenderTargetProperties(m_hwnd, D2D1::SizeU(rc.Width(), rc.Height())),
        &m_render_target
    );
    if (FAILED(hr)) return false;
    
    hr = m_render_target->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &m_brush);
    if (FAILED(hr)) return false;

    D2D1_STROKE_STYLE_PROPERTIES stroke_props = {};
    stroke_props.startCap = D2D1_CAP_STYLE_ROUND;
    stroke_props.endCap = D2D1_CAP_STYLE_ROUND;
    stroke_props.dashCap = D2D1_CAP_STYLE_ROUND;
    stroke_props.lineJoin = D2D1_LINE_JOIN_ROUND;
    stroke_props.dashStyle = D2D1_DASH_STYLE_SOLID;
    hr = m_d2d_factory->CreateStrokeStyle(stroke_props, NULL, 0, &m_round_stroke);
    if (FAILED(hr)) return false;

    return true;
}

bool D2DRenderer::begin_draw()
{
    if (!create_resources()) return false;
    
    m_render_target->BeginDraw();
    m_render_target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    m_render_target->SetTransform(D2D1::Matrix3x2F::Identity());
    
    return true;
}

void D2DRenderer::end_draw()
{
    if (m_render_target) {
        HRESULT hr = m_render_target->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET) {
            release_resources();
        }
    }
}

void D2DRenderer::clear_background(float opacity)
{
    // Dark semi-transparent background like game HUD
    m_render_target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, opacity));
}

void D2DRenderer::draw_rounded_rect(const D2D1_RECT_F& rect, float radius,
                                     const D2D1_COLOR_F& color, float opacity)
{
    D2D1_COLOR_F final_color = color;
    final_color.a *= opacity;
    
    m_brush->SetColor(final_color);
    
    m_render_target->FillRoundedRectangle(
        D2D1::RoundedRect(rect, radius, radius), m_brush);
}

void D2DRenderer::draw_text(const wchar_t* text, float x, float y, float width, float height,
                             IDWriteTextFormat* format, const D2D1_COLOR_F& color)
{
    if (!format) return;
    
    m_brush->SetColor(color);
    
    D2D1_RECT_F rect = D2D1::RectF(x, y, x + width, y + height);
    // CLIP keeps overflowing single-line text from drawing outside its box.
    m_render_target->DrawText(text, (UINT32)wcslen(text), format, rect, m_brush,
                              D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

float D2DRenderer::measure_text_width(const wchar_t* text, IDWriteTextFormat* format)
{
    if (!text || !format || !m_dwrite_factory) return 0.0f;

    CComPtr<IDWriteTextLayout> layout;
    // With NO_WRAP the layout reports the text's natural single-line width.
    HRESULT hr = m_dwrite_factory->CreateTextLayout(
        text, (UINT32)wcslen(text), format, 10000.0f, 1000.0f, &layout);
    if (FAILED(hr)) return 0.0f;

    DWRITE_TEXT_METRICS metrics = {};
    if (FAILED(layout->GetMetrics(&metrics))) return 0.0f;
    return metrics.widthIncludingTrailingWhitespace;
}

void D2DRenderer::draw_progress_bar(float x, float y, float width, float height,
                                     float progress, const D2D1_COLOR_F& fg_color,
                                     const D2D1_COLOR_F& bg_color)
{
    // Background
    m_brush->SetColor(bg_color);
    m_render_target->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(x, y, x + width, y + height), height / 2, height / 2), m_brush);
    
    // Foreground (progress)
    float fg_width = width * std::clamp(progress, 0.0f, 1.0f);
    if (fg_width >= height) { // Only draw if there's enough space
        m_brush->SetColor(fg_color);
        m_render_target->FillRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(x, y, x + fg_width, y + height), height / 2, height / 2), m_brush);
    }
}

void D2DRenderer::draw_album_art(float x, float y, float size, album_art_data_ptr art)
{
    if (art.is_empty() || !art->get_size()) return;
    
    // If we have a new album art, create a D2D bitmap from it
    if (m_album_art_dirty && m_wic_factory) {
        if (m_album_art_bitmap) {
            m_album_art_bitmap->Release();
            m_album_art_bitmap = nullptr;
        }
        
        // Create a WIC stream from the album art data
        IWICStream* wic_stream = nullptr;
        HRESULT hr = m_wic_factory->CreateStream(&wic_stream);
        if (SUCCEEDED(hr)) {
            hr = wic_stream->InitializeFromMemory(
                const_cast<BYTE*>(static_cast<const BYTE*>(art->get_ptr())),
                (DWORD)art->get_size());
            
            if (SUCCEEDED(hr)) {
                IWICBitmapDecoder* decoder = nullptr;
                hr = m_wic_factory->CreateDecoderFromStream(
                    wic_stream, NULL, WICDecodeMetadataCacheOnLoad, &decoder);
                
                if (SUCCEEDED(hr)) {
                    IWICBitmapFrameDecode* frame = nullptr;
                    hr = decoder->GetFrame(0, &frame);
                    
                    if (SUCCEEDED(hr)) {
                        IWICFormatConverter* converter = nullptr;
                        hr = m_wic_factory->CreateFormatConverter(&converter);
                        
                        if (SUCCEEDED(hr)) {
                            hr = converter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA,
                                                       WICBitmapDitherTypeNone, NULL, 0.0, 
                                                       WICBitmapPaletteTypeMedianCut);
                            
                            if (SUCCEEDED(hr)) {
                                hr = m_render_target->CreateBitmapFromWicBitmap(converter, NULL, &m_album_art_bitmap);
                            }
                            converter->Release();
                        }
                        frame->Release();
                    }
                    decoder->Release();
                }
            }
            wic_stream->Release();
        }
        
        m_album_art_dirty = false;
    }
    
    // Draw the bitmap
    if (m_album_art_bitmap) {
        m_render_target->DrawBitmap(m_album_art_bitmap, D2D1::RectF(x, y, x + size, y + size));
    } else {
        // Draw placeholder (music note icon as simple colored rect)
        m_brush->SetColor(D2D1::ColorF(0.3f, 0.3f, 0.3f, 0.8f));
        m_render_target->FillRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(x, y, x + size, y + size), 4, 4), m_brush);
    }
}

void D2DRenderer::draw_button(float x, float y, float size, const wchar_t* icon_text,
                               bool is_active, const D2D1_COLOR_F& color)
{
    // Circle background
    D2D1_COLOR_F bg_color = is_active ? 
        D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.3f) : 
        D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.15f);
    
    m_brush->SetColor(bg_color);
    float radius = size / 2;
    m_render_target->FillEllipse(
        D2D1::Ellipse(D2D1::Point2F(x + radius, y + radius), radius, radius), m_brush);
    
    // Icon text (simple Unicode symbols or just text)
    if (icon_text && wcslen(icon_text) > 0) {
        m_brush->SetColor(color);
        
        // Use default format for icons
        auto format = get_small_format();
        if (format) {
            D2D1_RECT_F text_rect = D2D1::RectF(x, y, x + size, y + size);
            m_render_target->DrawText(icon_text, (UINT32)wcslen(icon_text), 
                                       format, text_rect, m_brush,
                                       D2D1_DRAW_TEXT_OPTIONS_NONE);
        }
    }
}

void D2DRenderer::draw_progress_ring(float cx, float cy, float radius, float thickness,
                                      float progress, const D2D1_COLOR_F& fg_color,
                                      const D2D1_COLOR_F& bg_color)
{
    // Background track (full circle)
    m_brush->SetColor(bg_color);
    m_render_target->DrawEllipse(
        D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius), m_brush, thickness, m_round_stroke);

    progress = std::clamp(progress, 0.0f, 1.0f);
    if (progress <= 0.0f) return;      // nothing filled yet
    if (progress >= 1.0f) {            // complete -> full ring
        m_brush->SetColor(fg_color);
        m_render_target->DrawEllipse(
            D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius), m_brush, thickness, m_round_stroke);
        return;
    }

    // Progress arc from 12 o'clock, clockwise. Arc size must follow progress:
    // SMALL for <50%, LARGE for >=50% (a fixed LARGE previously showed ~90% at 10%).
    const float angle = progress * 2.0f * 3.14159265f;
    const D2D1_POINT_2F start_pt = D2D1::Point2F(cx + radius * sinf(0.0f), cy - radius * cosf(0.0f));
    const D2D1_POINT_2F end_pt = D2D1::Point2F(cx + radius * sinf(angle), cy - radius * cosf(angle));

    CComPtr<ID2D1PathGeometry> path_geo;
    CComPtr<ID2D1GeometrySink> sink;
    if (SUCCEEDED(m_d2d_factory->CreatePathGeometry(&path_geo)) &&
        SUCCEEDED(path_geo->Open(&sink))) {
        sink->SetFillMode(D2D1_FILL_MODE_WINDING);
        sink->BeginFigure(start_pt, D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddArc(D2D1::ArcSegment(
            end_pt,
            D2D1::SizeF(radius, radius),
            0.0f,
            D2D1_SWEEP_DIRECTION_CLOCKWISE,
            progress < 0.5f ? D2D1_ARC_SIZE_SMALL : D2D1_ARC_SIZE_LARGE));
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
        sink->Close();

        m_brush->SetColor(fg_color);
        m_render_target->DrawGeometry(path_geo, m_brush, thickness, m_round_stroke);
    }
}

IDWriteTextFormat* D2DRenderer::get_text_format(float size, DWRITE_FONT_WEIGHT weight)
{
    IDWriteTextFormat* format = nullptr;
    
    m_dwrite_factory->CreateTextFormat(
        L"Segoe UI", NULL, weight, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, size, L"en-US", &format);
    
    if (format) {
        format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        // Single line: never wrap (long text would overlap the next row).
        // Overflow is clipped in draw_text (DWRITE_TRIMMING_SIGN not available
        // under the current _WIN32_WINNT, so ellipsis trimming is omitted).
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
    
    return format;
}

IDWriteTextFormat* D2DRenderer::get_title_format()
{
    if (!m_title_format) {
        m_title_format = get_text_format(14.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        if (m_title_format) {
            m_title_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            m_title_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        }
    }
    return m_title_format;
}

IDWriteTextFormat* D2DRenderer::get_artist_format()
{
    if (!m_artist_format) {
        m_artist_format = get_text_format(11.0f, DWRITE_FONT_WEIGHT_NORMAL);
        if (m_artist_format) {
            m_artist_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            m_artist_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        }
    }
    return m_artist_format;
}

IDWriteTextFormat* D2DRenderer::get_small_format()
{
    if (!m_small_format) {
        m_small_format = get_text_format(10.0f, DWRITE_FONT_WEIGHT_NORMAL);
        if (m_small_format) {
            m_small_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_small_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
    }
    return m_small_format;
}
#include "stdafx.h"
#include "d2d_renderer.h"
#include "config.h"

// ============================================================================
// D2DRenderer implementation
// ============================================================================

namespace {
// Corner radius for album art, scaled to the display size (plan 011 follow-up):
// size/16 is just enough rounding to see on small thumbnails; large hero art
// keeps the surface card radius (16).
float art_corner_radius(float size) {
    return (std::min)((float)WINDOW_CORNER_RADIUS, (std::max)(4.0f, size * 0.003125f)); // 1/32
}
} // namespace

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
    
    // Create D2D 1.1 factory (also creates all 1.0-compatible objects).
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&m_d2d_factory));
    if (FAILED(hr)) { FB2K_console_formatter() << "Floating Clouds: D2D factory FAILED hr=0x" << pfc::format_hex(hr); return false; }
    
    // Create DWrite factory
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), 
                              reinterpret_cast<IUnknown**>(&m_dwrite_factory));
    if (FAILED(hr)) { FB2K_console_formatter() << "Floating Clouds: DWrite factory FAILED hr=0x" << pfc::format_hex(hr); return false; }
    
    // Create WIC factory
    hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                           IID_IWICImagingFactory, (void**)&m_wic_factory);
    if (FAILED(hr)) { FB2K_console_formatter() << "Floating Clouds: WIC factory FAILED hr=0x" << pfc::format_hex(hr); return false; }
    
    // Create the present target: UpdateLayeredWindow (per-pixel alpha) first,
    // Hwnd fallback second.
    bool ok = create_resources();
    if (!ok) console::print("Floating Clouds: render target creation FAILED");
    return ok;
}

void D2DRenderer::release_resources()
{
    // ULW per-pixel-alpha path
    if (m_shadow_bitmap) { m_shadow_bitmap->Release(); m_shadow_bitmap = nullptr; }
    if (m_ulw_target) { m_ulw_target->Release(); m_ulw_target = nullptr; }
    if (m_dib) { DeleteObject(m_dib); m_dib = nullptr; }
    if (m_mem_dc) { DeleteDC(m_mem_dc); m_mem_dc = nullptr; }
    m_dib_bits = nullptr;
    // Hwnd fallback target
    if (m_hwnd_target) { m_hwnd_target->Release(); m_hwnd_target = nullptr; }
    m_render_target = nullptr;
    // Shared resources
    if (m_brush) { m_brush->Release(); m_brush = nullptr; }
    if (m_title_format) { m_title_format->Release(); m_title_format = nullptr; }
    if (m_artist_format) { m_artist_format->Release(); m_artist_format = nullptr; }
    if (m_small_format) { m_small_format->Release(); m_small_format = nullptr; }
    if (m_small_left_format) { m_small_left_format->Release(); m_small_left_format = nullptr; }
    if (m_rounded_rect_geo) { m_rounded_rect_geo->Release(); m_rounded_rect_geo = nullptr; }
    if (m_round_stroke) { m_round_stroke->Release(); m_round_stroke = nullptr; }
    if (m_album_art_bitmap) { m_album_art_bitmap->Release(); m_album_art_bitmap = nullptr; }
    m_mode = PresentMode::None;
    m_rebuilds++;
    FB2K_console_formatter() << "Floating Clouds: present resources released (rebuild #" << (int)m_rebuilds << ")";
}

bool D2DRenderer::create_resources()
{
    if (m_render_target) return true;

    // Primary: UpdateLayeredWindow per-pixel alpha (driver-agnostic; AMD
    // DirectComposition per-pixel surfaces are known to flicker).
    if (create_ulw_resources()) {
        m_mode = PresentMode::ULW;
        return true;
    }

    // Clean up any partial ULW objects before the fallback.
    release_resources();

    // Fallback: uniform-alpha HwndRenderTarget (square corners).
    if (create_hwnd_resources()) {
        m_mode = PresentMode::Hwnd;
        FB2K_console_formatter() << "Floating Clouds: ULW unavailable, using Hwnd fallback";
        return true;
    }
    return false;
}

bool D2DRenderer::create_ulw_resources()
{
    if (!m_d2d_factory) return false;

    CRect rc;
    ::GetClientRect(m_hwnd, &rc);
    const int w = (std::max)(1L, (long)rc.Width());
    const int h = (std::max)(1L, (long)rc.Height());
    m_dib_size.cx = w;
    m_dib_size.cy = h;
    m_dib_rect = CRect(0, 0, w, h);

    // 32-bpp top-down DIB (premultiplied alpha for UpdateLayeredWindow).
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    m_mem_dc = CreateCompatibleDC(NULL);
    if (!m_mem_dc) return false;
    m_dib = CreateDIBSection(m_mem_dc, &bi, DIB_RGB_COLORS, &m_dib_bits, NULL, 0);
    if (!m_dib) return false;
    SelectObject(m_mem_dc, m_dib);

    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    HRESULT hr = m_d2d_factory->CreateDCRenderTarget(&props, &m_ulw_target);
    if (FAILED(hr)) return false;

    m_render_target = m_ulw_target;
    if (!create_brush_and_stroke()) return false;
    if (!create_shadow()) return false;
    FB2K_console_formatter() << "Floating Clouds: ULW DIB " << w << "x" << h;
    return true;
}

bool D2DRenderer::create_hwnd_resources()
{
    if (!m_d2d_factory) return false;

    CRect rc;
    ::GetClientRect(m_hwnd, &rc);

    HRESULT hr = m_d2d_factory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
        ),
        D2D1::HwndRenderTargetProperties(m_hwnd, D2D1::SizeU((UINT)(std::max)(1L, (long)rc.Width()),
                                                             (UINT)(std::max)(1L, (long)rc.Height()))),
        &m_hwnd_target
    );
    if (FAILED(hr)) return false;

    m_render_target = m_hwnd_target;
    return create_brush_and_stroke();
}

bool D2DRenderer::create_brush_and_stroke()
{
    if (!m_render_target) return false;

    HRESULT hr = m_render_target->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &m_brush);
    if (FAILED(hr)) return false;

    D2D1_STROKE_STYLE_PROPERTIES stroke_props = {};
    stroke_props.startCap = D2D1_CAP_STYLE_ROUND;
    stroke_props.endCap = D2D1_CAP_STYLE_ROUND;
    stroke_props.dashCap = D2D1_CAP_STYLE_ROUND;
    stroke_props.lineJoin = D2D1_LINE_JOIN_ROUND;
    stroke_props.dashStyle = D2D1_DASH_STYLE_SOLID;
    hr = m_d2d_factory->CreateStrokeStyle(stroke_props, NULL, 0, &m_round_stroke);
    return SUCCEEDED(hr);
}

bool D2DRenderer::create_shadow()
{
    if (!m_ulw_target || !m_dib_bits) return false;

    const int w = m_dib_size.cx;
    const int h = m_dib_size.cy;
    if (m_shadow_bitmap) { m_shadow_bitmap->Release(); m_shadow_bitmap = nullptr; }

    const float r = (float)WINDOW_CORNER_RADIUS;
    const int inset = SHADOW_INSET;
    const int x0 = inset, y0 = inset, x1 = w - inset, y1 = h - inset;

    // Rasterize the rounded-card alpha into a CPU buffer.
    std::vector<uint8_t> a((size_t)w * h, 0);
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            int dx = (std::max)(x0 - x, (std::max)(x - x1, 0));
            int dy = (std::max)(y0 - y, (std::max)(y - y1, 0));
            float d = (float)std::sqrt((double)(dx * dx + dy * dy));
            if (d <= r) a[(size_t)y * w + x] = 255;
        }
    }

    // Separable box blur (3 passes, radius 5) for a soft shadow.
    std::vector<uint8_t> tmp((size_t)w * h), tmp2((size_t)w * h);
    auto blur_h = [&](const std::vector<uint8_t>& src, std::vector<uint8_t>& dst, int rad) {
        for (int y = 0; y < h; y++) {
            long acc = 0;
            const size_t row = (size_t)y * w;
            for (int x = -rad; x <= rad; x++) {
                int xi = (std::max)(0, (std::min)(w - 1, x));
                acc += src[row + xi];
            }
            for (int x = 0; x < w; x++) {
                dst[row + x] = (uint8_t)(acc / (2 * rad + 1));
                int xi1 = (std::max)(0, (std::min)(w - 1, x - rad));
                int xi2 = (std::max)(0, (std::min)(w - 1, x + rad + 1));
                acc += src[row + xi2] - src[row + xi1];
            }
        }
    };
    auto blur_v = [&](const std::vector<uint8_t>& src, std::vector<uint8_t>& dst, int rad) {
        for (int x = 0; x < w; x++) {
            long acc = 0;
            for (int y = -rad; y <= rad; y++) {
                int yi = (std::max)(0, (std::min)(h - 1, y));
                acc += src[(size_t)yi * w + x];
            }
            for (int y = 0; y < h; y++) {
                dst[(size_t)y * w + x] = (uint8_t)(acc / (2 * rad + 1));
                int yi1 = (std::max)(0, (std::min)(h - 1, y - rad));
                int yi2 = (std::max)(0, (std::min)(h - 1, y + rad + 1));
                acc += src[(size_t)yi2 * w + x] - src[(size_t)yi1 * w + x];
            }
        }
    };
    for (int pass = 0; pass < 3; pass++) {
        blur_h(a, tmp, 5);
        blur_v(tmp, tmp2, 5);
        a.swap(tmp2);
    }

    // Upload as a premultiplied black bitmap (RGB=0, A = blurred alpha * 0.4).
    std::vector<uint32_t> px((size_t)w * h);
    for (size_t i = 0; i < (size_t)w * h; i++) {
        px[i] = (uint32_t)(a[i] * 0.40f) << 24; // B8G8R8A8: alpha in the high byte
    }
    HRESULT hr = m_ulw_target->CreateBitmap(D2D1::SizeU((UINT)w, (UINT)h), px.data(),
        (UINT)w * 4,
        D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
        &m_shadow_bitmap);
    return SUCCEEDED(hr);
}

bool D2DRenderer::begin_draw()
{
    if (!create_resources()) return false;

    m_marquee_active = false; // reset per frame

    if (m_mode == PresentMode::ULW) {
        if (!m_ulw_target || !m_mem_dc) return false;
        HRESULT hr = m_ulw_target->BindDC(m_mem_dc, &m_dib_rect);
        if (FAILED(hr)) { m_fail_begin++; return false; }
        m_ulw_target->BeginDraw();
        m_ulw_target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        m_ulw_target->SetTransform(D2D1::Matrix3x2F::Identity());
    } else {
        m_hwnd_target->BeginDraw();
        m_hwnd_target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        m_hwnd_target->SetTransform(D2D1::Matrix3x2F::Identity());
    }
    
    return true;
}

void D2DRenderer::end_draw()
{
    if (!m_render_target) return;

    const double t_frame0 = (double)GetTickCount64();
    HRESULT hr = m_render_target->EndDraw();
    const double t_render = (double)GetTickCount64();
    m_frames++;

    if (m_mode == PresentMode::ULW) {
        // Fold the global opacity into the premultiplied DIB pixels (scale all
        // four bytes), so fading never touches DWM per-pixel composition.
        if (m_dib_bits && m_global_opacity < 1.0f) {
            const uint32_t n = (uint32_t)((size_t)m_dib_size.cx * (size_t)m_dib_size.cy);
            const uint32_t o = (uint32_t)(m_global_opacity * 255.0f + 0.5f);
            uint8_t* p = (uint8_t*)m_dib_bits;
            for (uint32_t i = 0; i < n; i++, p += 4) {
                p[0] = (uint8_t)((p[0] * o) / 255);
                p[1] = (uint8_t)((p[1] * o) / 255);
                p[2] = (uint8_t)((p[2] * o) / 255);
                p[3] = (uint8_t)((p[3] * o) / 255);
            }
        }
        if (SUCCEEDED(hr) && m_mem_dc) {
            POINT pt_src = { 0, 0 };
            SIZE size = m_dib_size;
            BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
            ::UpdateLayeredWindow(m_hwnd, NULL, NULL, &size, m_mem_dc, &pt_src, 0, &blend, ULW_ALPHA);
        }
    } else if (hr == D2DERR_RECREATE_TARGET) {
        release_resources();
    }

    const double t_total = (double)GetTickCount64();
    if (debug_enabled() && (t_total - t_frame0) > 25.0) {
        FB2K_console_formatter() << "Floating Clouds: frame " << (int)(t_total - t_frame0) << "ms (render " << (int)(t_render - t_frame0) << ")";
    }

    log_present_summary();
}

void D2DRenderer::clear_background(float opacity)
{
    if (m_mode == PresentMode::ULW) {
        // Per-pixel alpha: clear to fully transparent; the rounded card + shadow
        // are drawn by draw_surface_card.
        m_render_target->Clear(D2D1::ColorF(0, 0, 0, 0));
    } else {
        // Uniform-alpha fallback: per-pixel alpha is ignored, so paint the MD3
        // surface-container tone here (LWA_ALPHA applies the window opacity).
        m_render_target->Clear(D2DRenderer::hex(md3::surface_container, opacity));
    }
}

void D2DRenderer::set_global_opacity(float opacity)
{
    // ULW applies the opacity at present time (per-pixel multiply in end_draw).
    m_global_opacity = opacity;
}

bool D2DRenderer::debug_enabled()
{
    cfg_var_modern::cfg_bool cfg_debug(cfg_guids::debug_logging, false);
    return cfg_debug.get();
}

void D2DRenderer::log_present_summary()
{
    if (!debug_enabled()) {
        m_frames = m_fail_begin = m_fail_commit = m_fail_surface = 0;
        m_report_t = 0.0;
        return;
    }
    const double now = (double)GetTickCount64();
    if (m_report_t == 0.0) { m_report_t = now; return; }
    if (now - m_report_t >= 1000.0) {
        FB2K_console_formatter() << "Floating Clouds: present frames=" << (int)m_frames
            << " beginFail=" << (int)m_fail_begin
            << " srfFail=" << (int)m_fail_surface
            << " commitFail=" << (int)m_fail_commit
            << " rebuilds=" << (int)m_rebuilds
            << " mode=" << (m_mode == PresentMode::ULW ? "ULW" : (m_mode == PresentMode::Hwnd ? "Hwnd" : "None"));
        m_report_t = now;
        m_frames = m_fail_begin = m_fail_commit = m_fail_surface = 0;
    }
}

void D2DRenderer::draw_surface_card(const D2D1_RECT_F& rect, float radius)
{
    if (m_mode == PresentMode::ULW) {
        // Elevation shadow: CPU box-blurred bitmap, drawn once per frame.
        if (m_shadow_bitmap) {
            m_render_target->DrawBitmap(m_shadow_bitmap,
                D2D1::RectF(0, 0, (float)m_dib_size.cx, (float)m_dib_size.cy),
                1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        }
        m_brush->SetColor(D2DRenderer::hex(md3::surface_container, 1.0f));
        m_render_target->FillRoundedRectangle(
            D2D1::RoundedRect(rect, radius, radius), m_brush);
    }
    // Hwnd fallback: clear_background already filled the opaque surface.
}

void D2DRenderer::on_resize(CSize size)
{
    if (m_mode == PresentMode::ULW) {
        if (!m_mem_dc) return;
        const int w = (std::max)(1L, (long)size.cx);
        const int h = (std::max)(1L, (long)size.cy);
        if (m_dib_size.cx == w && m_dib_size.cy == h) return;
        m_dib_size.cx = w;
        m_dib_size.cy = h;
        m_dib_rect = CRect(0, 0, w, h);

        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = w;
        bi.bmiHeader.biHeight = -h;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        HBITMAP new_dib = CreateDIBSection(m_mem_dc, &bi, DIB_RGB_COLORS, &m_dib_bits, NULL, 0);
        if (new_dib) {
            HBITMAP old = (HBITMAP)SelectObject(m_mem_dc, new_dib);
            if (old && old != new_dib) DeleteObject(old);
            m_dib = new_dib;
        }
        FB2K_console_formatter() << "Floating Clouds: ULW DIB resized to " << w << "x" << h;
        create_shadow();
    } else if (m_hwnd_target) {
        m_hwnd_target->Resize(D2D1::SizeU((UINT)(std::max)(1L, (long)size.cx),
                                          (UINT)(std::max)(1L, (long)size.cy)));
    }
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
    if (!format || !text) return;

    // Text wider than its box auto-scrolls (marquee) instead of clipping.
    if (width > 0) {
        float tw = measure_text_width(text, format);
        if (tw > width) {
            draw_text_marquee(text, x, y, width, height, format, color);
            return;
        }
    }

    m_brush->SetColor(color);
    
    D2D1_RECT_F rect = D2D1::RectF(x, y, x + width, y + height);
    // CLIP keeps overflowing single-line text from drawing outside its box.
    m_render_target->DrawText(text, (UINT32)wcslen(text), format, rect, m_brush,
                              D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void D2DRenderer::draw_text_marquee(const wchar_t* text, float x, float y, float width, float height,
                                     IDWriteTextFormat* format, const D2D1_COLOR_F& color)
{
    m_marquee_active = true;
    const float tw = measure_text_width(text, format);

    // Gentle marquee: pause, scroll left, pause, restart.
    const float gap = 24.0f;
    const double start_pause = 1000.0;  // ms before scrolling begins
    const double end_pause = 800.0;     // ms after the text has scrolled out
    const double speed_px_s = 28.0;
    const double travel = (double)tw + gap;
    const double scroll_ms = travel / speed_px_s * 1000.0;
    const double cycle = start_pause + scroll_ms + end_pause;

    const double phase = fmod((double)GetTickCount64(), cycle);
    float offset = 0.0f;
    if (phase >= start_pause + scroll_ms) {
        offset = (float)travel;   // end pause: text fully scrolled out
    } else if (phase >= start_pause) {
        offset = (float)((phase - start_pause) / scroll_ms * travel);
    }                             // start pause: offset = 0

    m_brush->SetColor(color);
    m_render_target->PushAxisAlignedClip(
        D2D1::RectF(x, y, x + width, y + height), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    D2D1_RECT_F rect = D2D1::RectF(x - offset, y, x - offset + 10000.0f, y + height);
    m_render_target->DrawText(text, (UINT32)wcslen(text), format, rect, m_brush,
                              D2D1_DRAW_TEXT_OPTIONS_CLIP);
    m_render_target->PopAxisAlignedClip();
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
    if (fg_width > 0.0f) {
        m_brush->SetColor(fg_color);
        if (fg_width < height) {
            // Tiny progress: draw a dot (capsule head) so the bar is never
            // empty at the very start of a track.
            const float dr = fg_width / 2.0f;
            m_render_target->FillEllipse(
                D2D1::Ellipse(D2D1::Point2F(x + dr, y + height / 2.0f), dr, dr), m_brush);
        } else {
            m_render_target->FillRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(x, y, x + fg_width, y + height), height / 2, height / 2), m_brush);
        }
    }
}

void D2DRenderer::draw_album_art(float x, float y, float size, album_art_data_ptr art)
{
    if (art.is_empty() || !art->get_size()) return;

    // Rebuild the D2D bitmap when a new image arrives OR when the display size
    // changes. The source is pre-scaled to the display size with WIC Fant
    // interpolation, so downscaling a large cover to a small tile doesn't alias
    // into moire patterns (plain D2D DrawBitmap LINEAR sampling does).
    if ((m_album_art_dirty || m_album_art_display_size != (int)size) && m_wic_factory) {
        if (m_album_art_bitmap) {
            m_album_art_bitmap->Release();
            m_album_art_bitmap = nullptr;
        }

        IWICStream* wic_stream = nullptr;
        if (SUCCEEDED(m_wic_factory->CreateStream(&wic_stream))) {
            if (SUCCEEDED(wic_stream->InitializeFromMemory(
                    const_cast<BYTE*>(static_cast<const BYTE*>(art->get_ptr())),
                    (DWORD)art->get_size()))) {

                IWICBitmapDecoder* decoder = nullptr;
                if (SUCCEEDED(m_wic_factory->CreateDecoderFromStream(
                        wic_stream, NULL, WICDecodeMetadataCacheOnLoad, &decoder))) {

                    IWICBitmapFrameDecode* frame = nullptr;
                    if (SUCCEEDED(decoder->GetFrame(0, &frame))) {

                        // Pre-scale down with Fant (best downscale quality) when
                        // the cover is larger than the display tile; otherwise
                        // keep the native size (upscaling stays soft, no moire).
                        IWICBitmapSource* src = frame;
                        CComPtr<IWICBitmapScaler> scaler;
                        UINT src_w = 0, src_h = 0;
                        frame->GetSize(&src_w, &src_h);
                        if (src_w > (UINT)size && src_h > (UINT)size) {
                            if (SUCCEEDED(m_wic_factory->CreateBitmapScaler(&scaler)) &&
                                SUCCEEDED(scaler->Initialize(frame, (UINT)size, (UINT)size,
                                    WICBitmapInterpolationModeFant))) {
                                src = scaler;
                            }
                        }

                        IWICFormatConverter* converter = nullptr;
                        if (SUCCEEDED(m_wic_factory->CreateFormatConverter(&converter))) {
                            if (SUCCEEDED(converter->Initialize(src, GUID_WICPixelFormat32bppPBGRA,
                                    WICBitmapDitherTypeNone, NULL, 0.0,
                                    WICBitmapPaletteTypeMedianCut))) {
                                // Round the corners once at build time (plan 011): copy to a
                                // CPU buffer, apply a rounded-rect alpha mask (radius = the
                                // surface card radius), then upload. Baked into the cached
                                // bitmap -> zero per-frame cost, safe for the ULW software
                                // render target.
                                UINT aw = 0, ah = 0;
                                converter->GetSize(&aw, &ah);
                                std::vector<uint32_t> buf((size_t)aw * ah);
                                const float r = art_corner_radius((float)((std::min)(aw, ah)));
                                const UINT rr = (UINT)(r * 2.0f);
                                if (aw >= rr && ah >= rr &&
                                    SUCCEEDED(converter->CopyPixels(NULL, aw * 4, aw * ah * 4,
                                        (BYTE*)buf.data()))) {
                                    const float rx = (float)(aw - 1);
                                    const float ry = (float)(ah - 1);
                                    for (UINT py = 0; py < ah; py++) {
                                        const float fpy = (float)py;
                                        for (UINT px = 0; px < aw; px++) {
                                            const float fpx = (float)px;
                                            const float qx = fpx - (std::min)((std::max)(fpx, r), rx - r);
                                            const float qy = fpy - (std::min)((std::max)(fpy, r), ry - r);
                                            const float d = sqrtf(qx * qx + qy * qy) - r; // <0 inside
                                            const float cov = (std::max)(0.0f, (std::min)(1.0f, 0.5f - d));
                                            // PBGRA is PREMULTIPLIED: scaling only the alpha byte while
                                            // leaving RGB would wash the edge pixels into a faded corner.
                                            // Scale all four bytes so the silhouette is truly cut.
                                            const uint32_t cov255 = (uint32_t)(cov * 255.0f + 0.5f);
                                            uint32_t& p = buf[(size_t)py * aw + px];
                                            uint8_t* pxb = (uint8_t*)&p;
                                            pxb[0] = (uint8_t)((pxb[0] * cov255) / 255);
                                            pxb[1] = (uint8_t)((pxb[1] * cov255) / 255);
                                            pxb[2] = (uint8_t)((pxb[2] * cov255) / 255);
                                            pxb[3] = (uint8_t)((pxb[3] * cov255) / 255);
                                        }
                                    }
                                    m_render_target->CreateBitmap(
                                        D2D1::SizeU(aw, ah), buf.data(), aw * 4,
                                        D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                                                                                 D2D1_ALPHA_MODE_PREMULTIPLIED)),
                                        &m_album_art_bitmap);
                                } else {
                                    // Too small to mask (or copy failed): keep the plain bitmap.
                                    m_render_target->CreateBitmapFromWicBitmap(converter, NULL, &m_album_art_bitmap);
                                }
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
        m_album_art_display_size = (int)size;
    }

    // Draw the (already correctly sized) bitmap
    if (m_album_art_bitmap) {
        m_render_target->DrawBitmap(m_album_art_bitmap, D2D1::RectF(x, y, x + size, y + size),
                                    1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    } else {
        // Draw placeholder (music note icon as simple colored rect)
        m_brush->SetColor(D2DRenderer::hex(md3::surface_container_high, 0.9f));
        m_render_target->FillRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(x, y, x + size, y + size),
                              art_corner_radius(size), art_corner_radius(size)), m_brush);
    }
}

void D2DRenderer::draw_button(float x, float y, float size, const wchar_t* icon_text,
                               bool is_active, const D2D1_COLOR_F& color, int state, float state_alpha)
{
    const float cx = x + size / 2.0f;
    const float cy = y + size / 2.0f;
    const float radius = size / 2.0f;

    // Press feedback (plan 001): shrink toward the button center, driven by the
    // eased pressed-state layer so the scale animates with the state layer.
    const bool pressed = (state >= 2);
    const float scale = pressed ? 1.0f - 0.03f * (state_alpha / md3::pressed_state) : 1.0f;
    const float rr = radius * scale;

    // Circle background (MD3 icon-button tonal fill)
    D2D1_COLOR_F bg_color = is_active ? 
        D2DRenderer::hex(md3::primary, 0.20f) : 
        D2DRenderer::hex(md3::on_surface_variant, 0.12f);
    
    m_brush->SetColor(bg_color);
    m_render_target->FillEllipse(
        D2D1::Ellipse(D2D1::Point2F(cx, cy), rr, rr), m_brush);

    // MD3 state layer: hover 8% / pressed 12% on-surface overlay, eased.
    if (state_alpha > 0.001f) {
        m_brush->SetColor(D2DRenderer::hex(md3::on_surface, state_alpha));
        m_render_target->FillEllipse(
            D2D1::Ellipse(D2D1::Point2F(cx, cy), rr, rr), m_brush);
    }
    
    // Icon text (simple Unicode symbols or just text), scaled with the button
    if (icon_text && wcslen(icon_text) > 0) {
        m_brush->SetColor(color);
        
        // Use default format for icons
        auto format = get_small_format();
        if (format) {
            D2D1_RECT_F text_rect = D2D1::RectF(cx - rr, cy - rr, cx + rr, cy + rr);
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

IDWriteTextFormat* D2DRenderer::get_small_left_format()
{
    if (!m_small_left_format) {
        m_small_left_format = get_text_format(10.0f, DWRITE_FONT_WEIGHT_NORMAL);
        if (m_small_left_format) {
            m_small_left_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            m_small_left_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
    }
    return m_small_left_format;
}
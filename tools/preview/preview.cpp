// README preview renderer (tools/preview).
//
// Renders the floating card at 2x from the SAME code the component uses
// (skin_tokens.h + styles/glass_card.h + styles/icons.h), so the previews in
// imgs/ cannot drift from the shipped look. A 2x render is done by scaling the
// pixel-valued tokens (corner radius, shadow inset/blur/offset, type sizes) and
// the layout constants - not by upscaling a 1x image - which keeps the
// anti-aliasing of the rounded corners and the glyphs crisp.
//
// Run from the repository root so the relative output paths land in imgs/:
//     tools\preview\build_preview.cmd   (from a VS developer prompt, or see below)
//     preview.exe
// Output: imgs/skins.png, imgs/MD3/card-{dark,light}.png,
//         imgs/Mist/card-{dark,light}.png
// It needs no foobar2000 SDK: only Direct2D, DirectWrite and WIC.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <atlbase.h>
#include <cstdio>
#include <cmath>
#include "../../foo_floating_clouds/dynamic_color.h"
#include "../../foo_floating_clouds/styles/glass_card.h"
#include "../../foo_floating_clouds/styles/icons.h"

static const float kZoom = 2.0f;
static const int kCardW = 384, kCardH = 124;

struct Geom {
    int inset, winW, winH, cardW, cardH;
    float zoom;
};

static Geom make_geom(float zoom)
{
    Geom g;
    g.zoom = zoom;
    g.inset = (int)(8 * zoom + 0.5f);
    g.cardW = (int)(kCardW * zoom + 0.5f);
    g.cardH = (int)(kCardH * zoom + 0.5f);
    g.winW = g.cardW + 2 * g.inset;
    g.winH = g.cardH + 2 * g.inset;
    return g;
}

// Scale every pixel-valued token so the painter draws at `zoom`.
static SkinTokens scaled(const SkinTokens& in, float z)
{
    SkinTokens t = in;
    t.corner_card *= z;
    t.shadow_inset *= z;
    t.shadow_blur = (int)(t.shadow_blur * z + 0.5f);
    t.shadow_contact_blur = (int)(t.shadow_contact_blur * z + 0.5f);
    t.shadow_offset_y *= z;
    t.title_size *= z;
    t.artist_size *= z;
    t.small_size *= z;
    return t;
}

static bool save_png(IWICImagingFactory* wic, IWICBitmap* bmp, const wchar_t* path)
{
    CComPtr<IWICStream> stream;
    if (FAILED(wic->CreateStream(&stream))) return false;
    if (FAILED(stream->InitializeFromFilename(path, GENERIC_WRITE))) return false;
    CComPtr<IWICBitmapEncoder> enc;
    if (FAILED(wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc))) return false;
    if (FAILED(enc->Initialize(stream, WICBitmapEncoderNoCache))) return false;
    CComPtr<IWICBitmapFrameEncode> frame;
    CComPtr<IPropertyBag2> props;
    if (FAILED(enc->CreateNewFrame(&frame, &props))) return false;
    if (FAILED(frame->Initialize(props))) return false;
    if (FAILED(frame->WriteSource(bmp, nullptr))) return false;
    frame->Commit();
    enc->Commit();
    return true;
}

// Content: a cover tile, title/artist, progress and the control row - the same
// layout StyleRenderer::render_full produces, tinted by the active tokens.
static void paint_content(ID2D1RenderTarget* rt, ID2D1Factory* df, IDWriteFactory* dw,
                          const SkinTokens& t, float zoom, const Geom& g)
{
    CComPtr<ID2D1SolidColorBrush> b;
    rt->CreateSolidColorBrush(D2D1::ColorF(0, 0), &b);

    const float pad = 12.0f * zoom;
    const float inset = (float)g.inset;
    const float art = 64.0f * zoom;
    const float x = inset + pad, y = inset + pad;

    // album art: a warm cover-like gradient
    CComPtr<ID2D1GradientStopCollection> gc;
    CComPtr<ID2D1LinearGradientBrush> gb;
    const D2D1_GRADIENT_STOP gs[2] = { { 0.0f, D2D1::ColorF(0x9A3038) }, { 1.0f, D2D1::ColorF(0x2B1B26) } };
    if (SUCCEEDED(rt->CreateGradientStopCollection(gs, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &gc)) &&
        SUCCEEDED(rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(
            D2D1::Point2F(x, y), D2D1::Point2F(x, y + art)), gc, &gb))) {
        rt->FillRoundedRectangle(D2D1::RoundedRect(
            D2D1::RectF(x, y, x + art, y + art), 9.0f * zoom, 9.0f * zoom), gb);
    }

    CComPtr<IDWriteTextFormat> title, artist;
    dw->CreateTextFormat(L"Microsoft YaHei", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                         DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, t.title_size, L"zh-CN", &title);
    dw->CreateTextFormat(L"Microsoft YaHei", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                         DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, t.artist_size, L"zh-CN", &artist);
    const float tx = x + art + pad;
    const float tw = (float)g.cardW - pad * 3.0f - art;
    if (title) {
        title->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        b->SetColor(D2D1::ColorF(t.on_surface, 0.95f));
        const wchar_t* s = L"\u5927\u5c55\u9e3f\u56fe";
        rt->DrawText(s, (UINT32)wcslen(s), title, D2D1::RectF(tx, y - 1, tx + tw, y + 22 * zoom), b);
    }
    if (artist) {
        artist->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        b->SetColor(D2D1::ColorF(t.on_surface_variant, 0.85f));
        const wchar_t* s = L"\u63fd\u4f6cSKAI ISYOURGOD & AR\u5218\u592b\u9633";
        rt->DrawText(s, (UINT32)wcslen(s), artist, D2D1::RectF(tx, y + 23 * zoom, tx + tw, y + 40 * zoom), b);
    }

    const float py = y + 48.0f * zoom;
    b->SetColor(D2D1::ColorF(t.progress_track, t.progress_track_alpha));
    rt->FillRoundedRectangle(D2D1::RoundedRect(
        D2D1::RectF(tx, py, tx + tw, py + 4 * zoom), 2 * zoom, 2 * zoom), b);
    b->SetColor(D2D1::ColorF(t.primary, 0.9f));
    rt->FillRoundedRectangle(D2D1::RoundedRect(
        D2D1::RectF(tx, py, tx + tw * 0.34f, py + 4 * zoom), 2 * zoom, 2 * zoom), b);

    // control row: prev, play/pause (active), next, volume, queue
    const icons::Kind kinds[5] = { icons::Kind::Prev, icons::Kind::Pause, icons::Kind::Next,
                                   icons::Kind::VolumeOn, icons::Kind::Queue };
    const float btn = 24.0f * zoom;
    const float gap = 8.0f * zoom;
    const float row_w = 5 * btn + 4 * gap;
    const float bx = (float)g.winW * 0.5f - row_w * 0.5f;
    const float by = (float)(g.inset + g.cardH) - 12.0f * zoom - btn;
    for (int i = 0; i < 5; i++) {
        const float cx = bx + i * (btn + gap) + btn * 0.5f;
        const float cy = by + btn * 0.5f;
        const bool active = (i == 1);
        b->SetColor(active ? D2D1::ColorF(t.button_fill_active, t.button_fill_active_alpha)
                           : D2D1::ColorF(t.button_fill, t.button_fill_alpha));
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), btn * 0.5f, btn * 0.5f), b);
        CComPtr<ID2D1PathGeometry> geo;
        if (icons::build_unit_geometry(df, kinds[i], &geo)) {
            const D2D1_COLOR_F col = active ? D2D1::ColorF(t.on_button_active, 1.0f)
                                            : D2D1::ColorF(t.on_surface_variant, 0.95f);
            icons::draw(rt, geo, b, D2D1::Point2F(cx, cy), btn * 0.70f, col);
        }
    }
}

// Draw one card into an existing target at (ox, oy) using `tokens`.
static void draw_card(ID2D1RenderTarget* rt, ID2D1Factory* df, IDWriteFactory* dw,
                      const SkinTokens& tokens, const D2D1_POINT_2F& origin, float zoom)
{
    const Geom g = make_geom(zoom);
    const SkinTokens t = scaled(tokens, zoom);

    CComPtr<ID2D1Bitmap> sh, ct;
    glass::build_shadow_bitmap(rt, g.winW, g.winH, t, t.shadow_alpha, t.shadow_blur, &sh);
    if (t.shadow_contact_alpha > 0.0f) {
        glass::build_shadow_bitmap(rt, g.winW, g.winH, t, t.shadow_contact_alpha,
                                   t.shadow_contact_blur, &ct);
    }

    const D2D1_RECT_F rect = D2D1::RectF((float)g.inset, (float)g.inset,
                                         (float)(g.inset + g.cardW), (float)(g.inset + g.cardH));
    glass::CardGeometry geo;
    glass::build_card_geometry(df, rect, t.corner_card, t.superellipse, 1.0f * zoom, geo);

    rt->SetTransform(D2D1::Matrix3x2F::Translation(origin.x, origin.y));
    glass::paint_card(rt, rect, t, geo, sh, ct, D2D1::SizeF((float)g.winW, (float)g.winH));
    paint_content(rt, df, dw, t, zoom, g);
    rt->SetTransform(D2D1::Matrix3x2F::Identity());
}

int wmain()
{
    {
        CComPtr<IWICImagingFactory> wic;
        CComPtr<ID2D1Factory> d2d;
        CComPtr<IDWriteFactory> dwrite;
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic));
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory),
                          reinterpret_cast<void**>(&d2d));
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                            reinterpret_cast<IUnknown**>(&dwrite));

        const Geom g = make_geom(kZoom);

        struct Item { const char* file; const SkinTokens* tokens; };
        // Single cards, saved on their own for the per-skin rows in the README.
        const Item items[] = {
            { "imgs/MD3/card-dark.png",   &md3_skin },
            { "imgs/MD3/card-light.png",  &md3_skin_light },
            { "imgs/Mist/card-dark.png",  &mist_skin },
            { "imgs/Mist/card-light.png", &mist_skin_light },
        };
        for (const Item& it : items) {
            CComPtr<IWICBitmap> bmp;
            wic->CreateBitmap(g.winW, g.winH, GUID_WICPixelFormat32bppPBGRA,
                              WICBitmapCreateCacheOption::WICBitmapCacheOnLoad, &bmp);
            CComPtr<ID2D1RenderTarget> rt;
            const D2D1_RENDER_TARGET_PROPERTIES rtp = D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
            d2d->CreateWicBitmapRenderTarget(bmp, rtp, &rt);
            rt->BeginDraw();
            rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            rt->Clear(D2D1::ColorF(0x3A3A40));
            draw_card(rt, d2d, dwrite, *it.tokens, D2D1::Point2F(0, 0), kZoom);
            rt->EndDraw();
            rt.Release();
            wchar_t wide[64] = {};
            MultiByteToWideChar(CP_ACP, 0, it.file, -1, wide, 64);
            std::printf("%s %s\n", save_png(wic, bmp, wide) ? "wrote" : "FAILED", it.file);
        }

        // 2x2 grid: the hero image for both READMEs.
        const int pad = 10;
        const int W = g.winW * 2 + pad * 3, H = g.winH * 2 + pad * 3;
        CComPtr<IWICBitmap> bmp;
        wic->CreateBitmap(W, H, GUID_WICPixelFormat32bppPBGRA,
                          WICBitmapCreateCacheOption::WICBitmapCacheOnLoad, &bmp);
        CComPtr<ID2D1RenderTarget> rt;
        const D2D1_RENDER_TARGET_PROPERTIES rtp = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        d2d->CreateWicBitmapRenderTarget(bmp, rtp, &rt);
        rt->BeginDraw();
        rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        rt->Clear(D2D1::ColorF(0x5A5A62));
        draw_card(rt, d2d, dwrite, md3_skin, D2D1::Point2F((float)pad, (float)pad), kZoom);
        draw_card(rt, d2d, dwrite, md3_skin_light,
                  D2D1::Point2F((float)(pad * 2 + g.winW), (float)pad), kZoom);
        draw_card(rt, d2d, dwrite, mist_skin,
                  D2D1::Point2F((float)pad, (float)(pad * 2 + g.winH)), kZoom);
        draw_card(rt, d2d, dwrite, mist_skin_light,
                  D2D1::Point2F((float)(pad * 2 + g.winW), (float)(pad * 2 + g.winH)), kZoom);
        rt->EndDraw();
        rt.Release();
        std::printf("%s imgs/skins.png (%dx%d)\n",
                    save_png(wic, bmp, L"imgs/skins.png") ? "wrote" : "FAILED", W, H);
    }
    CoUninitialize();
    return 0;
}

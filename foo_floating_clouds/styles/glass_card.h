#pragma once

// ============================================================================
// glass_card.h - the floating surface card: shape, elevation shadow, material.
//
// Pure Direct2D: no foobar2000 types, so a standalone probe can render the
// exact same pixels as the component (see logs/glass/probe).
//
// Why the card is built this way
// ------------------------------
// The window presents through UpdateLayeredWindow with per-pixel alpha, so:
//   * there is no backdrop blur available (a layered window's pixels are
//     composited by DWM, they cannot sample what is behind them);
//   * every pixel that is not part of the card must have alpha 0, which makes
//     any stroke centred on the silhouette edge a visible outline: half of its
//     width lands on transparent pixels and reads as a black (light mode) or
//     white (dark mode) sticker edge around all four sides.
// So the rim is NOT stroked. The card is filled in layers, and the rim is a
// 1px ring produced by filling (outer path XOR inner path) - it can only ever
// cover pixels inside the silhouette.
//
// The shadow has the same constraint from the other side: the card is inset
// from the window edge by shadow_inset, so a blur wider than that margin is
// clipped by the window boundary and turns into a hard rectangular ridge.
// shadow_blur is therefore kept small enough to fit, and the mask is feathered
// to zero at the window border with a smoothstep so the tail dies invisibly.
// ============================================================================

#include <d2d1.h>
#include <d2d1helper.h>
#include <cmath>
#include <cstdint>
#include <vector>

#include "../skin_tokens.h"

namespace glass {

// Superellipse exponent for the continuous "squircle" corner. 2 = circular arc.
constexpr float kSquircleN = 5.0f;

inline float card_radius(float radius, const D2D1_RECT_F& rect)
{
    const float w = rect.right - rect.left;
    const float h = rect.bottom - rect.top;
    const float half = 0.5f * (w < h ? w : h);
    return radius < half ? radius : half;
}

// ---------------------------------------------------------------------------
// Outline sampling: straight edges plus four corners. A circular corner is the
// superellipse with n = 2, so one code path covers both shapes.
//
// Each corner is described by its centre, the direction of its quadrant
// (ex/ey), and how its two quarter-circle components map to x and y. The
// direction lives in ex/ey ONLY: the trig factors are always magnitudes in
// [0,1], otherwise the two signs cancel on the left-hand corners and the
// outline folds inwards.
// ---------------------------------------------------------------------------
inline void collect_outline(std::vector<D2D1_POINT_2F>& pts, const D2D1_RECT_F& r,
                            float rad, bool squircle)
{
    const float n = squircle ? kSquircleN : 2.0f;
    const int samples = squircle ? 12 : 8;
    const float pi = 3.14159265358979f;

    struct Corner { float cx, cy, ex, ey; bool x_is_sin; };
    const Corner corners[4] = {
        { r.right - rad, r.top + rad,    +1.0f, -1.0f, true  }, // top-right
        { r.right - rad, r.bottom - rad, +1.0f, +1.0f, false }, // bottom-right
        { r.left + rad,  r.bottom - rad, -1.0f, +1.0f, true  }, // bottom-left
        { r.left + rad,  r.top + rad,    -1.0f, -1.0f, false }, // top-left
    };

    pts.clear();
    pts.reserve(4 * (samples + 1) + 1);
    pts.push_back(D2D1::Point2F(r.left + rad, r.top));
    for (int c = 0; c < 4; c++) {
        const Corner& k = corners[c];
        for (int i = 0; i <= samples; i++) {
            const float a = (pi * 0.5f) * (float)i / (float)samples;
            // cosf(pi/2) is a tiny NEGATIVE float, and powf(negative, 2/n) is
            // NaN - one NaN point makes the whole geometry degenerate and
            // FillGeometry silently draws nothing. Clamp to [0,1] first.
            float cs = cosf(a), sn = sinf(a);
            cs = cs < 0.0f ? 0.0f : cs;
            sn = sn < 0.0f ? 0.0f : sn;
            const float s = powf(sn, 2.0f / n);
            const float t = powf(cs, 2.0f / n);
            const float fx = k.x_is_sin ? s : t;
            const float fy = k.x_is_sin ? t : s;
            pts.push_back(D2D1::Point2F(k.cx + k.ex * rad * fx, k.cy + k.ey * rad * fy));
        }
    }
}

inline void add_figure(ID2D1GeometrySink* sink, const std::vector<D2D1_POINT_2F>& pts)
{
    sink->BeginFigure(pts[0], D2D1_FIGURE_BEGIN_FILLED);
    for (size_t i = 1; i < pts.size(); i++) sink->AddLine(pts[i]);
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
}

// Cached card geometry: `outer` is the silhouette, `ring` is the 1px inner rim
// (outer XOR inner, drawn with the alternate fill rule).
struct CardGeometry {
    ID2D1PathGeometry* outer = nullptr;
    ID2D1PathGeometry* ring = nullptr;

    void release()
    {
        if (outer) { outer->Release(); outer = nullptr; }
        if (ring) { ring->Release(); ring = nullptr; }
    }
    ~CardGeometry() { release(); }
};

inline bool build_card_geometry(ID2D1Factory* factory, const D2D1_RECT_F& rect,
                                float radius, bool squircle, float rim_width,
                                CardGeometry& out)
{
    out.release();
    if (!factory) return false;

    const float rad = card_radius(radius, rect);
    std::vector<D2D1_POINT_2F> pts;

    // Silhouette.
    ID2D1PathGeometry* outer = nullptr;
    if (FAILED(factory->CreatePathGeometry(&outer)) || !outer) return false;
    {
        ID2D1GeometrySink* sink = nullptr;
        if (FAILED(outer->Open(&sink)) || !sink) { outer->Release(); return false; }
        sink->SetFillMode(D2D1_FILL_MODE_WINDING);
        collect_outline(pts, rect, rad, squircle);
        add_figure(sink, pts);
        if (FAILED(sink->Close())) { sink->Release(); outer->Release(); return false; }
        sink->Release();
    }

    // Rim ring: silhouette + inset silhouette, alternate fill => the 1px band.
    ID2D1PathGeometry* ring = nullptr;
    if (FAILED(factory->CreatePathGeometry(&ring)) || !ring) { outer->Release(); return false; }
    {
        ID2D1GeometrySink* sink = nullptr;
        if (FAILED(ring->Open(&sink)) || !sink) { ring->Release(); outer->Release(); return false; }
        sink->SetFillMode(D2D1_FILL_MODE_ALTERNATE);
        collect_outline(pts, rect, rad, squircle);
        add_figure(sink, pts);

        const D2D1_RECT_F inner = D2D1::RectF(rect.left + rim_width, rect.top + rim_width,
                                              rect.right - rim_width, rect.bottom - rim_width);
        collect_outline(pts, inner, card_radius(rad - rim_width, inner), squircle);
        add_figure(sink, pts);

        if (FAILED(sink->Close())) { sink->Release(); ring->Release(); outer->Release(); return false; }
        sink->Release();
    }

    out.outer = outer;
    out.ring = ring;
    return true;
}

// ---------------------------------------------------------------------------
// Elevation shadow: CPU mask (same silhouette math) -> box blur -> premultiplied
// black bitmap. Feathered to zero at the window border so the square window
// boundary never shows up as a rectangular edge.
// ---------------------------------------------------------------------------
inline bool inside_card(float x, float y, const D2D1_RECT_F& r, float rad, bool squircle)
{
    if (x < r.left || x > r.right || y < r.top || y > r.bottom) return false;
    if (rad <= 0.0f) return true;

    const float qx = (std::max)(r.left + rad - x, x - (r.right - rad));
    const float qy = (std::max)(r.top + rad - y, y - (r.bottom - rad));
    if (qx <= 0.0f || qy <= 0.0f) return true;   // straight part of an edge

    const float n = squircle ? kSquircleN : 2.0f;
    const float u = qx / rad, v = qy / rad;
    return powf(u, n) + powf(v, n) <= 1.0f;
}

inline bool build_shadow_bitmap(ID2D1RenderTarget* rt, int w, int h,
                                const SkinTokens& t, float alpha, int blur,
                                ID2D1Bitmap** out_bitmap)
{
    if (!rt || !out_bitmap) return false;
    *out_bitmap = nullptr;
    if (alpha <= 0.0f) return true;

    const int inset = (int)t.shadow_inset;
    const D2D1_RECT_F card = D2D1::RectF((float)inset, (float)inset + t.shadow_offset_y,
                                         (float)(w - inset), (float)(h - inset) + t.shadow_offset_y);
    const float rad = card_radius(t.corner_card, card);

    // 1) Rasterize the card mask (hard edges are fine: the blur follows).
    std::vector<uint8_t> a((size_t)w * h, 0);
    const int y0 = (std::max)(0, (int)card.top);
    const int y1 = (std::min)(h, (int)card.bottom + 1);
    const int x0 = (std::max)(0, (int)card.left);
    const int x1 = (std::min)(w, (int)card.right + 1);
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            if (inside_card((float)x + 0.5f, (float)y + 0.5f, card, rad, t.superellipse)) {
                a[(size_t)y * w + x] = 255;
            }
        }
    }

    // 2) Three box-blur passes ~ a gaussian (sigma = sqrt(blur*blur + blur)).
    if (blur > 0) {
        std::vector<uint8_t> tmp((size_t)w * h), tmp2((size_t)w * h);
        const int denom = 2 * blur + 1;
        for (int pass = 0; pass < 3; pass++) {
            for (int y = 0; y < h; y++) {
                long acc = 0;
                const size_t row = (size_t)y * w;
                for (int x = -blur; x <= blur; x++) {
                    acc += a[row + (std::max)(0, (std::min)(w - 1, x))];
                }
                for (int x = 0; x < w; x++) {
                    tmp[row + x] = (uint8_t)(acc / denom);
                    acc += a[row + (std::max)(0, (std::min)(w - 1, x + blur + 1))];
                    acc -= a[row + (std::max)(0, (std::min)(w - 1, x - blur))];
                }
            }
            for (int x = 0; x < w; x++) {
                long acc = 0;
                for (int y = -blur; y <= blur; y++) {
                    acc += tmp[(size_t)(std::max)(0, (std::min)(h - 1, y)) * w + x];
                }
                for (int y = 0; y < h; y++) {
                    tmp2[(size_t)y * w + x] = (uint8_t)(acc / denom);
                    acc += tmp[(size_t)(std::max)(0, (std::min)(h - 1, y + blur + 1)) * w + x];
                    acc -= tmp[(size_t)(std::max)(0, (std::min)(h - 1, y - blur)) * w + x];
                }
            }
            a.swap(tmp2);
        }
    }

    // 3) Upload premultiplied black, feathered to zero at the window border.
    const float feather = (float)(std::max)(1, inset);
    std::vector<uint32_t> px((size_t)w * h);
    for (int y = 0; y < h; y++) {
        const float dy = (float)(std::min)(y, h - 1 - y);
        for (int x = 0; x < w; x++) {
            const float dx = (float)(std::min)(x, w - 1 - x);
            float edge = (dx < dy ? dx : dy) / feather;
            edge = edge > 1.0f ? 1.0f : edge;
            edge = edge * edge * (3.0f - 2.0f * edge);   // smoothstep
            const size_t i = (size_t)y * w + x;
            px[i] = (uint32_t)((float)a[i] * alpha * edge) << 24; // B8G8R8A8 alpha
        }
    }

    HRESULT hr = rt->CreateBitmap(
        D2D1::SizeU((UINT)w, (UINT)h), px.data(), (UINT)w * 4,
        D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                                                 D2D1_ALPHA_MODE_PREMULTIPLIED)),
        out_bitmap);
    return SUCCEEDED(hr);
}

// ---------------------------------------------------------------------------
// Paint. Order matters: elevation, body, inner rim, top gloss, bottom depth.
// ---------------------------------------------------------------------------
inline void fill_gradient(ID2D1RenderTarget* rt, ID2D1PathGeometry* geo,
                          const D2D1_RECT_F& rect, const D2D1_GRADIENT_STOP* stops, UINT32 count)
{
    if (!rt || !geo) return;
    ID2D1GradientStopCollection* coll = nullptr;
    if (FAILED(rt->CreateGradientStopCollection(stops, count, D2D1_GAMMA_2_2,
                                                D2D1_EXTEND_MODE_CLAMP, &coll)) || !coll) {
        return;
    }
    ID2D1LinearGradientBrush* brush = nullptr;
    if (SUCCEEDED(rt->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(D2D1::Point2F(rect.left, rect.top),
                                                D2D1::Point2F(rect.left, rect.bottom)),
            coll, &brush)) && brush) {
        rt->FillGeometry(geo, brush);
        brush->Release();
    }
    coll->Release();
}

inline void paint_card(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, const SkinTokens& t,
                       const CardGeometry& geo, ID2D1Bitmap* shadow, ID2D1Bitmap* contact,
                       const D2D1_SIZE_F& surface)
{
    if (!rt || !geo.outer) return;

    // 1) Elevation (contact first, then the wide soft layer).
    const D2D1_RECT_F full = D2D1::RectF(0.0f, 0.0f, surface.width, surface.height);
    if (contact) rt->DrawBitmap(contact, full, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    if (shadow)  rt->DrawBitmap(shadow, full, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

    if (!t.glass) {
        // Opaque MD3 card: a single flat fill, exactly as before.
        ID2D1SolidColorBrush* brush = nullptr;
        if (SUCCEEDED(rt->CreateSolidColorBrush(D2D1::ColorF(t.surface, 1.0f), &brush)) && brush) {
            rt->FillGeometry(geo.outer, brush);
            brush->Release();
        }
        return;
    }

    // Vertical stop positions are derived from PIXEL distances, then clamped, so
    // the same material works on a 124px card and on a tall playlist panel: a
    // fraction of the height would turn the top bloom into a 140px white wash on
    // a 500px surface.
    const float h = rect.bottom - rect.top;
    const float rim_px = (std::min)(10.0f, h * 0.08f);
    const float gloss_px = (std::max)(16.0f, (std::min)(44.0f, h * t.card_gloss_stop));
    const float depth_px = (std::max)(8.0f, (std::min)(30.0f, h * (1.0f - t.card_depth_stop)));
    const float rim_t = h > 0.0f ? rim_px / h : 0.05f;
    const float gloss_t = h > 0.0f ? gloss_px / h : 0.3f;
    const float depth_t = h > 0.0f ? 1.0f - depth_px / h : 0.86f;

    // 2) Body: shallow vertical gradient, lit at the top.
    const D2D1_GRADIENT_STOP body[2] = {
        { 0.0f, D2D1::ColorF(t.card_fill_top, t.card_fill_top_alpha) },
        { 1.0f, D2D1::ColorF(t.card_fill_bottom, t.card_fill_bottom_alpha) },
    };
    fill_gradient(rt, geo.outer, rect, body, 2);

    // 3) Rim: 1px ring inside the silhouette. Bright top edge, shaded bottom
    //    edge, nothing at mid height (a full-perimeter rim is what makes a card
    //    look like a sticker).
    const D2D1_GRADIENT_STOP rim[5] = {
        { 0.00f, D2D1::ColorF(t.card_rim_light, t.card_rim_light_alpha) },
        { rim_t * 0.35f, D2D1::ColorF(t.card_rim_light, t.card_rim_light_alpha * 0.25f) },
        { rim_t, D2D1::ColorF(t.card_rim_light, 0.0f) },
        { 1.0f - rim_t, D2D1::ColorF(t.card_rim_shade, 0.0f) },
        { 1.00f, D2D1::ColorF(t.card_rim_shade, t.card_rim_shade_alpha) },
    };
    fill_gradient(rt, geo.ring, rect, rim, 5);

    // 4) Specular bloom under the top edge (light source).
    if (t.card_gloss_alpha > 0.0f && gloss_t > 0.0f) {
        const D2D1_GRADIENT_STOP gloss[2] = {
            { 0.0f, D2D1::ColorF(t.card_gloss, t.card_gloss_alpha) },
            { gloss_t, D2D1::ColorF(t.card_gloss, 0.0f) },
        };
        fill_gradient(rt, geo.outer, rect, gloss, 2);
    }

    // 5) Inner depth shadow at the bottom edge (glass thickness).
    if (t.card_depth_alpha > 0.0f && depth_t < 1.0f) {
        const D2D1_GRADIENT_STOP depth[2] = {
            { depth_t, D2D1::ColorF(t.card_depth, 0.0f) },
            { 1.0f, D2D1::ColorF(t.card_depth, t.card_depth_alpha) },
        };
        fill_gradient(rt, geo.outer, rect, depth, 2);
    }
}

} // namespace glass

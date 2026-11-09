#pragma once

// ============================================================================
// glass_lens.h - the Liquid Glass body: a captured backdrop, lensed.
//
// This is the one material that cannot be expressed as gradient fills. It needs
// the pixels behind the window, so the picture is:
//
//   capture (backdrop.h) -> blur -> veil -> per-pixel refraction + dispersion
//   -> rim / specular / gloss / inner depth -> premultiplied BGRA -> D2D bitmap
//
// The model is the one the C++ references in SDK/ use (see
// design-plans/17-liquid-glass-skin.md and refs-liquid-glass-physics.md): the
// card is a slab of glass with a bevel of `lens_width` px; inside the bevelled
// band the surface normal tilts toward the silhouette, so the backdrop is
// sampled `lens_power` px OUTSIDE its nominal position, which squeezes the
// outside world into the rim. The centre of the card has no normal tilt, so it
// stays undistorted (just frosted) - that is what keeps the text readable.
//
// Pure CPU on purpose: the present target is an UpdateLayeredWindow DIB driven
// by ID2D1DCRenderTarget, which has no effect pipeline (that needs an
// ID2D1DeviceContext over a D3D11 device). The pipeline is arranged so the
// expensive per-pixel work only happens on the rim:
//
//   1. blur      one pass over the padded capture
//   2. veil      one pass over the padded capture (saturation + adaptive tint)
//   3. raster    interior = copy one pixel; rim = 1-3 bilinear taps + lighting
//
// Measured on this machine at 96 DPI: 400x140 card ~= 0.9 ms, 480x560 panel
// ~= 3.2 ms (Release, /O2). The renderer refreshes the material on a timer, not
// every frame, so a 60 fps content animation pays this only a few times a second.
//
// No Direct2D and no foobar2000 types here, so the standalone probe in
// logs/glass/probe/lens can render the identical pixels over a test backdrop.
// ============================================================================

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>

#include "../skin_tokens.h"

// windows.h defines min/max as macros; this header uses std::min/std::max (and
// is included both from the plugin, where windows.h came first, and from the
// standalone probe). Undefining them here keeps the standard names available
// for the rest of the translation unit, which is what the surrounding code
// already assumes by writing (std::min)(...).
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace glass {

// The captured desktop, padded by `pad` px on every side so the lens has real
// pixels to sample beyond the silhouette. BGRA (B,G,R,A bytes), top-down.
struct Backdrop {
    const uint32_t* px = nullptr;
    int w = 0;   // window width in px (the capture is w + 2*pad wide)
    int h = 0;
    int pad = 0;

    bool valid() const { return px != nullptr && w > 0 && h > 0; }
    int pitch() const { return w + 2 * pad; }
};

// Reusable buffers. The renderer owns one of these so a refresh never allocates.
struct LensScratch {
    std::vector<uint32_t> blur;    // padded capture, blurred (straight BGRA)
    std::vector<uint32_t> veiled;  // padded capture, blurred + saturation + veil
    std::vector<uint32_t> tmp;     // box-blur ping-pong
    std::vector<uint32_t> body;    // window-sized premultiplied BGRA (the result)
    bool veiled_valid = false;     // blur + veil done for this geometry/tokens
};

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int channel_of(uint32_t p, int shift) { return (int)((p >> shift) & 0xFFu); }

inline float luma01(int r, int g, int b)
{
    return (0.299f * (float)r + 0.587f * (float)g + 0.114f * (float)b) * (1.0f / 255.0f);
}

// One separable box pass on 32-bit BGRA.
//
// The divisor is turned into a fixed-point reciprocal once per pass: four
// integer divisions per pixel (one per channel) dominated this whole module -
// blur was 1.8 ms of a 3.8 ms 400x140 build before this change.
inline uint32_t div_rcp(int v, uint32_t inv16)
{
    return (uint32_t)((v * (int)inv16 + 32768) >> 16);
}

inline void box_pass_h(const uint32_t* src, uint32_t* dst, int w, int h, int r)
{
    if (r <= 0) {
        for (size_t i = 0, n = (size_t)w * h; i < n; i++) dst[i] = src[i];
        return;
    }
    const int denom = 2 * r + 1;
    const uint32_t inv16 = 65536u / (uint32_t)denom;
    for (int y = 0; y < h; y++) {
        const uint32_t* row = src + (size_t)y * w;
        uint32_t* out = dst + (size_t)y * w;
        int sb = 0, sg = 0, sr = 0, sa = 0;
        for (int x = -r; x <= r; x++) {
            const int cx = std::max(0, std::min(w - 1, x));
            const uint32_t p = row[cx];
            sb += channel_of(p, 0); sg += channel_of(p, 8);
            sr += channel_of(p, 16); sa += channel_of(p, 24);
        }
        for (int x = 0; x < w; x++) {
            out[x] = div_rcp(sb, inv16) | (div_rcp(sg, inv16) << 8) |
                     (div_rcp(sr, inv16) << 16) | (div_rcp(sa, inv16) << 24);
            const int add = std::max(0, std::min(w - 1, x + r + 1));
            const int sub = std::max(0, std::min(w - 1, x - r));
            const uint32_t pa = row[add], ps = row[sub];
            sb += channel_of(pa, 0) - channel_of(ps, 0);
            sg += channel_of(pa, 8) - channel_of(ps, 8);
            sr += channel_of(pa, 16) - channel_of(ps, 16);
            sa += channel_of(pa, 24) - channel_of(ps, 24);
        }
    }
}

inline void box_pass_v(const uint32_t* src, uint32_t* dst, int w, int h, int r)
{
    if (r <= 0) {
        for (size_t i = 0, n = (size_t)w * h; i < n; i++) dst[i] = src[i];
        return;
    }
    const int denom = 2 * r + 1;
    const uint32_t inv16 = 65536u / (uint32_t)denom;
    for (int x = 0; x < w; x++) {
        int sb = 0, sg = 0, sr = 0, sa = 0;
        for (int y = -r; y <= r; y++) {
            const int cy = std::max(0, std::min(h - 1, y));
            const uint32_t p = src[(size_t)cy * w + x];
            sb += channel_of(p, 0); sg += channel_of(p, 8);
            sr += channel_of(p, 16); sa += channel_of(p, 24);
        }
        for (int y = 0; y < h; y++) {
            dst[(size_t)y * w + x] =
                div_rcp(sb, inv16) | (div_rcp(sg, inv16) << 8) |
                (div_rcp(sr, inv16) << 16) | (div_rcp(sa, inv16) << 24);
            const int add = std::max(0, std::min(h - 1, y + r + 1));
            const int sub = std::max(0, std::min(h - 1, y - r));
            const uint32_t pa = src[(size_t)add * w + x], ps = src[(size_t)sub * w + x];
            sb += channel_of(pa, 0) - channel_of(ps, 0);
            sg += channel_of(pa, 8) - channel_of(ps, 8);
            sr += channel_of(pa, 16) - channel_of(ps, 16);
            sa += channel_of(pa, 24) - channel_of(ps, 24);
        }
    }
}

// Three box passes ~= one gaussian (an h+v pair, then a horizontal finish).
// Cheap, and on a blurred backdrop indistinguishable from a wider kernel. More
// passes buy almost nothing visually here: the lens, not the frost, is what the
// eye reads at the rim, so the budget goes to sharpness instead.
inline void blur_bgra(const uint32_t* src, int w, int h, int r,
                      std::vector<uint32_t>& scratch, std::vector<uint32_t>& dst)
{
    const size_t n = (size_t)w * h;
    scratch.resize(n);
    dst.resize(n);
    if (r <= 0) {
        std::copy(src, src + n, dst.begin());
        return;
    }
    box_pass_h(src, scratch.data(), w, h, r);
    box_pass_v(scratch.data(), dst.data(), w, h, r);
    box_pass_h(dst.data(), scratch.data(), w, h, r);
    std::copy(scratch.begin(), scratch.end(), dst.begin());
}

// Bilinear sample of one channel, with edge clamping.
inline float sample1(const uint32_t* px, int w, int h, float x, float y, int shift)
{
    const float fx = x - 0.5f;
    const float fy = y - 0.5f;
    const int x0 = (int)std::floor(fx);
    const int y0 = (int)std::floor(fy);
    const float tx = fx - (float)x0;
    const float ty = fy - (float)y0;
    const int xa = std::max(0, std::min(w - 1, x0));
    const int xb = std::max(0, std::min(w - 1, x0 + 1));
    const int ya = std::max(0, std::min(h - 1, y0));
    const int yb = std::max(0, std::min(h - 1, y0 + 1));
    const float p00 = (float)channel_of(px[(size_t)ya * w + xa], shift);
    const float p10 = (float)channel_of(px[(size_t)ya * w + xb], shift);
    const float p01 = (float)channel_of(px[(size_t)yb * w + xa], shift);
    const float p11 = (float)channel_of(px[(size_t)yb * w + xb], shift);
    const float top = p00 + (p10 - p00) * tx;
    const float bot = p01 + (p11 - p01) * tx;
    return top + (bot - top) * ty;
}

// Bilinear sample of all three colour channels (one weight computation).
inline void sample3(const uint32_t* px, int w, int h, float x, float y,
                    float& r, float& g, float& b)
{
    const float fx = x - 0.5f;
    const float fy = y - 0.5f;
    const int x0 = (int)std::floor(fx);
    const int y0 = (int)std::floor(fy);
    const float tx = fx - (float)x0;
    const float ty = fy - (float)y0;
    const int xa = std::max(0, std::min(w - 1, x0));
    const int xb = std::max(0, std::min(w - 1, x0 + 1));
    const int ya = std::max(0, std::min(h - 1, y0));
    const int yb = std::max(0, std::min(h - 1, y0 + 1));
    const uint32_t p00 = px[(size_t)ya * w + xa], p10 = px[(size_t)ya * w + xb];
    const uint32_t p01 = px[(size_t)yb * w + xa], p11 = px[(size_t)yb * w + xb];
    const float w00 = (1.0f - tx) * (1.0f - ty), w10 = tx * (1.0f - ty);
    const float w01 = (1.0f - tx) * ty, w11 = tx * ty;
    r = w00 * (float)channel_of(p00, 16) + w10 * (float)channel_of(p10, 16) +
        w01 * (float)channel_of(p01, 16) + w11 * (float)channel_of(p11, 16);
    g = w00 * (float)channel_of(p00, 8) + w10 * (float)channel_of(p10, 8) +
        w01 * (float)channel_of(p01, 8) + w11 * (float)channel_of(p11, 8);
    b = w00 * (float)channel_of(p00, 0) + w10 * (float)channel_of(p10, 0) +
        w01 * (float)channel_of(p01, 0) + w11 * (float)channel_of(p11, 0);
}

// ---------------------------------------------------------------------------
// Silhouette: signed distance (px, positive inside) + outward unit normal.
//
// Two shapes, matching glass_card.h: a plain rounded rect (circular corners)
// and a superellipse ("squircle") of exponent 5, which is what the glass skins
// use. Both are exact enough inside the lens band, which is all this material
// reads: the band is <= 20 px while the radius is >= 20 px.
// ---------------------------------------------------------------------------
struct ShapeHit {
    float d = -1.0f;    // signed distance, positive inside
    float nx = 0.0f;    // outward normal
    float ny = 0.0f;
};

inline ShapeHit shape_at(float x, float y, float cx, float cy, float a, float b,
                         float rad, bool squircle, float squircle_n)
{
    ShapeHit out;
    if (squircle) {
        const float X = (x - cx) / a;
        const float Y = (y - cy) / b;
        const float ax = std::fabs(X);
        const float ay = std::fabs(Y);
        const float ax2 = ax * ax, ay2 = ay * ay;
        const float ax4 = ax2 * ax2, ay4 = ay2 * ay2;
        const float F = ax4 * ax + ay4 * ay - 1.0f;
        // Gradient of the implicit surface, then first-order distance and
        // normal. Exact on the boundary, good enough just inside it.
        float gx = squircle_n * ax4 * (X < 0.0f ? -1.0f : 1.0f) / a;
        float gy = squircle_n * ay4 * (Y < 0.0f ? -1.0f : 1.0f) / b;
        const float g = std::sqrt(gx * gx + gy * gy);
        if (g < 1e-6f) { out.d = 1e3f; return out; }
        out.nx = gx / g;
        out.ny = gy / g;
        out.d = -F / g;
        return out;
    }

    const float qx = std::fabs(x - cx) - (a - rad);
    const float qy = std::fabs(y - cy) - (b - rad);
    const float ax = std::max(qx, 0.0f);
    const float ay = std::max(qy, 0.0f);
    const float len = std::sqrt(ax * ax + ay * ay);
    const float dstd = len + std::min(std::max(qx, qy), 0.0f) - rad;  // negative inside
    out.d = -dstd;
    if (len > 1e-6f) {
        out.nx = (x < cx ? -1.0f : 1.0f) * ax / len;
        out.ny = (y < cy ? -1.0f : 1.0f) * ay / len;
    } else {
        // Straight section: the normal points along whichever edge is nearest.
        out.nx = (qx > qy) ? (x < cx ? -1.0f : 1.0f) : 0.0f;
        out.ny = (qx > qy) ? 0.0f : (y < cy ? -1.0f : 1.0f);
    }
    return out;
}

// Light direction, in window space, pointing from the surface toward the light
// source (up and to the left, the same side the rim highlight has always used).
constexpr float kLightX = -0.42f;
constexpr float kLightY = -0.78f;

struct LensResult {
    int w = 0;
    int h = 0;
    bool ok = false;
    // Mean luminance (0..1) of the blurred backdrop under the card, and whether
    // the material flipped to light glass + dark ink. The renderer uses `bright`
    // to pick the ink for everything drawn on top of the card in this frame.
    float mean_luma = 0.0f;
    bool bright = false;
};

// ---------------------------------------------------------------------------
// Stage 1: blur. Stage 2a: mean luminance over an arbitrary rect (used to pick
// the polarity). Stage 2b: bake saturation + the adaptive veil. Splitting them
// keeps the blur to exactly one pass per refresh.
// ---------------------------------------------------------------------------
inline bool blur_backdrop(const Backdrop& bd, const SkinTokens& t, LensScratch& s, int bw, int bh)
{
    blur_bgra(bd.px, bw, bh, (int)(t.lens_frost + 0.5f), s.tmp, s.blur);
    return s.blur.size() == (size_t)bw * bh;
}

inline float mean_luma_rect(const uint32_t* px, int w, int h,
                            int x0, int y0, int x1, int y1, int step)
{
    if (x1 <= x0 || y1 <= y0) return 0.0f;
    if (step < 1) step = 1;
    double sum = 0.0;
    long n = 0;
    for (int y = y0; y < y1; y += step) {
        for (int x = x0; x < x1; x += step) {
            const uint32_t p = px[(size_t)y * w + x];
            sum += (double)luma01(channel_of(p, 16), channel_of(p, 8), channel_of(p, 0));
            n++;
        }
    }
    return n > 0 ? (float)(sum / (double)n) : 0.0f;
}

inline void apply_veil(const SkinTokens& t, bool bright, LensScratch& s, int bw, int bh)
{
    // Which veil: the dark one, or the white one the flipped glass needs.
    const uint32_t tint_hex = bright ? t.lens_tint_bright : t.lens_tint;
    const float tint_r = (float)((tint_hex >> 16) & 0xFF);
    const float tint_g = (float)((tint_hex >> 8) & 0xFF);
    const float tint_b = (float)(tint_hex & 0xFF);
    const float base = bright ? t.lens_tint_bright_alpha : t.lens_tint_alpha;
    const float gain = bright ? t.lens_tint_bright_gain : t.lens_tint_gain;
    const float sat = t.lens_saturation;

    s.veiled.resize((size_t)bw * bh);
    for (int y = 0; y < bh; y++) {
        const uint32_t* in = &s.blur[(size_t)y * bw];
        uint32_t* out = &s.veiled[(size_t)y * bw];
        for (int x = 0; x < bw; x++) {
            const uint32_t p = in[x];
            float r = (float)channel_of(p, 16);
            float g = (float)channel_of(p, 8);
            float b = (float)channel_of(p, 0);
            const float lum = (0.299f * r + 0.587f * g + 0.114f * b) * (1.0f / 255.0f);
            const float l255 = lum * 255.0f;
            r = l255 + (r - l255) * sat;
            g = l255 + (g - l255) * sat;
            b = l255 + (b - l255) * sat;
            const float veil = clampf(base + gain * lum, 0.0f, 0.94f);
            const float inv = 1.0f - veil;
            r = r * inv + tint_r * veil;
            g = g * inv + tint_g * veil;
            b = b * inv + tint_b * veil;
            out[x] = (uint32_t)clampf(b + 0.5f, 0.0f, 255.0f) |
                     ((uint32_t)clampf(g + 0.5f, 0.0f, 255.0f) << 8) |
                     ((uint32_t)clampf(r + 0.5f, 0.0f, 255.0f) << 16) | 0xFF000000u;
        }
    }
    s.veiled_valid = true;
}

// ---------------------------------------------------------------------------
// Stage 3: rasterize the card. `body` is window-sized premultiplied BGRA - zero
// outside the silhouette, lensed backdrop + lighting inside it.
// ---------------------------------------------------------------------------
inline LensResult build_lens_body(int win_w, int win_h, const Backdrop& bd,
                                  float card_l, float card_t, float card_r, float card_b,
                                  float radius, bool squircle, const SkinTokens& t,
                                  LensScratch& s, int polarity_hint = -1)
{
    LensResult res;
    if (win_w <= 0 || win_h <= 0 || !bd.valid()) return res;
    if (!t.glass_backdrop || t.lens_width <= 0.0f) return res;

    const int bw = bd.w + 2 * bd.pad;
    const int bh = bd.h + 2 * bd.pad;
    const int pad = bd.pad;

    const float cw = card_r - card_l;
    const float ch = card_b - card_t;
    if (cw < 4.0f || ch < 4.0f) return res;
    const float cx = 0.5f * (card_l + card_r);
    const float cy = 0.5f * (card_t + card_b);
    const float a = 0.5f * cw;
    const float b = 0.5f * ch;
    float rad = radius < std::min(a, b) ? radius : std::min(a, b);
    if (rad < 0.0f) rad = 0.0f;
    const float squircle_n = squircle ? 5.0f : 2.0f;

    // The band is clamped so a short card (the 48 px Minimal line) gets a bevel,
    // not a fully distorted surface; the bend is scaled with it.
    float band_w = t.lens_width;
    const float band_max = 0.58f * std::min(a, b);
    if (band_w > band_max) band_w = band_max;
    if (band_w < 1.0f) band_w = 1.0f;
    float power = t.lens_power;
    // A is bounded by the band: the references run A/H up to 2.0, which at our
    // card size would make the whole rim a smear. 0.62 keeps A reachable (the
    // previous 0.52 cap silently clamped A to 9.4 for H=18).
    if (power > 0.62f * band_w) power = 0.62f * band_w;

    // Polarity: dark backdrop -> dark glass + white ink; bright backdrop ->
    // light glass + dark ink (vibrancy). Decided from the blurred backdrop
    // under the card, before the veil is baked.
    if (!blur_backdrop(bd, t, s, bw, bh)) return res;
    const int mx0 = std::max(0, (int)card_l), mx1 = std::min(bw - 1, (int)card_r);
    const int my0 = std::max(0, (int)card_t), my1 = std::min(bh - 1, (int)card_b);
    const float mean_luma = mean_luma_rect(s.blur.data(), bw, bh, mx0, my0, mx1, my1, 3);
    // `polarity_hint` >= 0 comes from the renderer, which applies hysteresis so a
    // card hovering on the threshold cannot strobe between the two polarities.
    const bool bright = polarity_hint >= 0 ? (polarity_hint != 0)
                                           : (t.adaptive_ink && mean_luma > t.ink_threshold);

    apply_veil(t, bright, s, bw, bh);
    if (s.veiled.size() != (size_t)bw * bh) return res;
    const uint32_t* src = s.veiled.data();

    // Veil colours for the rim path, which applies the veil itself (the
    // interior reads it pre-applied from `s.veiled`).
    const uint32_t tint_hex = bright ? t.lens_tint_bright : t.lens_tint;
    const float tint_r = (float)((tint_hex >> 16) & 0xFF);
    const float tint_g = (float)((tint_hex >> 8) & 0xFF);
    const float tint_b = (float)(tint_hex & 0xFF);
    const float tint_alpha = bright ? t.lens_tint_bright_alpha : t.lens_tint_alpha;
    const float tint_gain = bright ? t.lens_tint_bright_gain : t.lens_tint_gain;
    const float sat = t.lens_saturation;

    const float rim_lr = (float)((t.card_rim_light >> 16) & 0xFF);
    const float rim_lg = (float)((t.card_rim_light >> 8) & 0xFF);
    const float rim_lb = (float)(t.card_rim_light & 0xFF);
    const float rim_sr = (float)((t.card_rim_shade >> 16) & 0xFF);
    const float rim_sg = (float)((t.card_rim_shade >> 8) & 0xFF);
    const float rim_sb = (float)(t.card_rim_shade & 0xFF);
    const float gloss_r = (float)((t.card_gloss >> 16) & 0xFF);
    const float gloss_g = (float)((t.card_gloss >> 8) & 0xFF);
    const float gloss_b = (float)(t.card_gloss & 0xFF);
    const float gloss_px = std::max(10.0f, std::min(40.0f, ch * t.card_gloss_stop));
    const float body_alpha = clampf(t.lens_alpha, 0.0f, 1.0f);
    const float spec_str = t.lens_specular;
    const float disp = t.lens_dispersion;
    const uint32_t* raw = bd.px;

    s.body.assign((size_t)win_w * win_h, 0u);
    const int x0 = std::max(0, (int)std::floor(card_l) - 1);
    const int y0 = std::max(0, (int)std::floor(card_t) - 1);
    const int x1 = std::min(win_w, (int)std::ceil(card_r) + 1);
    const int y1 = std::min(win_h, (int)std::ceil(card_b) + 1);

    for (int y = y0; y < y1; y++) {
        uint32_t* row = &s.body[(size_t)y * win_w];
        for (int x = x0; x < x1; x++) {
            const float px = (float)x + 0.5f;
            const float py = (float)y + 0.5f;
            const ShapeHit sh = shape_at(px, py, cx, cy, a, b, rad, squircle, squircle_n);
            if (sh.d < -1.0f) continue;

            // 1 px analytic coverage on the silhouette (AA rim, no staircase).
            float cov = clampf(sh.d + 0.5f, 0.0f, 1.0f);
            if (cov <= 0.0f) continue;

            const int sx = x + pad, sy = y + pad;
            const float inside = clampf(sh.d, 0.0f, band_w);
            const float band = 1.0f - inside / band_w;   // 0 inside -> 1 at edge
            float r, g, bl;

            if (band < 0.02f) {
                // Interior fast path: no tilt, so read the pre-veiled backdrop
                // exactly where the pixel is - one load, no bilinear.
                const uint32_t p = src[(size_t)sy * bw + sx];
                bl = (float)channel_of(p, 0);
                g = (float)channel_of(p, 8);
                r = (float)channel_of(p, 16);
            } else {
                // Lens curve. Both SDK references sample INWARD: the offset runs
                // against the outward normal, toward the card centre, so the rim
                // magnifies the content under the glass instead of wrapping the
                // world outside it. `1 - sqrt(1 - t^2)` is their curve (0 at the
                // inner end, 1 at the silhouette); a plain t^2 puts ~1.9x too
                // much bend in the middle of the band.
                float t = band;
                if (t > 1.0f) t = 1.0f;
                const float k2 = 1.0f - t * t;
                const float prof = 1.0f - std::sqrt(k2 > 0.0f ? k2 : 0.0f);
                const float m = power * prof;
                const float dx = -sh.nx * m;
                const float dy = -sh.ny * m;
                const float ds = disp * prof;

                if (ds > 0.0005f) {
                    r = sample1(s.blur.data(), bw, bh, (float)sx + dx * (1.0f + ds),
                                (float)sy + dy * (1.0f + ds), 16);
                    g = sample1(s.blur.data(), bw, bh, (float)sx + dx, (float)sy + dy, 8);
                    bl = sample1(s.blur.data(), bw, bh, (float)sx + dx * (1.0f - ds),
                                 (float)sy + dy * (1.0f - ds), 0);
                } else {
                    sample3(s.blur.data(), bw, bh, (float)sx + dx, (float)sy + dy, r, g, bl);
                }

                // The references blur the rim ~3x LESS than the centre, which is
                // what makes the bevel read as a lens rather than as more frost.
                // Blending the sharp capture back in only inside the band gets
                // that for the cost of one extra tap.
                float rr, gg, bb;
                sample3(raw, bw, bh, (float)sx + dx, (float)sy + dy, rr, gg, bb);
                const float sharp = 0.55f * t * t;
                r += (rr - r) * sharp;
                g += (gg - g) * sharp;
                bl += (bb - bl) * sharp;

                // Veil, applied here (the interior already has it baked in).
                const float lum = luma01((int)r, (int)g, (int)bl);
                const float l255 = lum * 255.0f;
                r = l255 + (r - l255) * sat;
                g = l255 + (g - l255) * sat;
                bl = l255 + (bl - l255) * sat;
                const float veil = clampf(tint_alpha + tint_gain * lum, 0.0f, 0.94f);
                const float inv = 1.0f - veil;
                r = r * inv + tint_r * veil;
                g = g * inv + tint_g * veil;
                bl = bl * inv + tint_b * veil;
            }

            const float band2 = band * band;
            const float up = sh.ny < 0.0f ? -sh.ny : 0.0f;     // faces the light
            const float down = sh.ny > 0.0f ? sh.ny : 0.0f;    // faces away

            // Specular: the light catches the bevelled edge. 8th power by
            // squaring (no powf in the inner loop).
            float white = 0.0f;
            if (spec_str > 0.0f) {
                const float ndl = sh.nx * kLightX + sh.ny * kLightY;
                if (ndl > 0.0f) {
                    const float s2 = ndl * ndl, s4 = s2 * s2;
                    white = s4 * s4 * spec_str * (0.25f + 0.75f * band2);
                }
            }

            // 1 px inner rim: bright where the normal faces up, shaded below.
            const float ring = clampf(1.0f - inside / 1.2f, 0.0f, 1.0f);
            const float rim_w = ring * (up * 0.85f + 0.15f) * t.card_rim_light_alpha;
            const float shade = ring * down * t.card_rim_shade_alpha;

            // Wide specular bloom under the top edge; inner depth at the bottom.
            const float gloss = up * clampf(1.0f - inside / gloss_px, 0.0f, 1.0f) * t.card_gloss_alpha;
            white += gloss;
            const float depth = down * band * t.card_depth_alpha;

            // Compose in [0,255]: shade -> rim -> white -> depth.
            r = r * (1.0f - shade) + rim_sr * shade;
            g = g * (1.0f - shade) + rim_sg * shade;
            bl = bl * (1.0f - shade) + rim_sb * shade;
            r = r * (1.0f - rim_w) + rim_lr * rim_w;
            g = g * (1.0f - rim_w) + rim_lg * rim_w;
            bl = bl * (1.0f - rim_w) + rim_lb * rim_w;
            if (white > 0.0f) {
                const float inv = white > 0.985f ? 0.0f : 1.0f - white;
                r = r * inv + gloss_r * white;
                g = g * inv + gloss_g * white;
                bl = bl * inv + gloss_b * white;
            }
            const float dp = 1.0f - depth;
            r *= dp; g *= dp; bl *= dp;

            // Premultiply into the output.
            const float A = cov * body_alpha;
            const uint32_t a8 = (uint32_t)clampf(A * 255.0f + 0.5f, 0.0f, 255.0f);
            row[x] = (uint32_t)clampf(bl * A + 0.5f, 0.0f, 255.0f) |
                     ((uint32_t)clampf(g * A + 0.5f, 0.0f, 255.0f) << 8) |
                     ((uint32_t)clampf(r * A + 0.5f, 0.0f, 255.0f) << 16) |
                     (a8 << 24);
        }
    }

    res.w = win_w;
    res.h = win_h;
    res.ok = true;
    res.mean_luma = mean_luma;
    res.bright = bright;
    return res;
}

} // namespace glass

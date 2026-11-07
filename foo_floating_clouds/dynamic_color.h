#pragma once

// ============================================================================
// dynamic_color.h - MD3 dynamic colour: a full scheme from one seed colour.
//
// Material's own implementation works in HCT (CAM16 hue/chroma + L* tone). This
// is the same idea in OKLab/OKLCh, which is close enough for a UI overlay and
// much shorter: the seed gives a HUE, every MD3 role is then placed at its
// specified TONE with a chroma budget that shrinks toward black and white (the
// gamut cannot hold saturated colour at the ends). Because roles are pinned to
// tones, the light/dark pairs (primary/on-primary, surface/on-surface, ...) keep
// their contrast whatever the cover art looks like - which is the whole reason
// MD3 defines them as tones rather than as fixed colours.
//
// Self-test: seed #6750A4 with these tones reproduces the shipped baseline
// scheme in md3_theme.h (surface-container #211F26, primary #D0BCFF, ...).
// ============================================================================

#include <cmath>
#include <cstdint>
#include <algorithm>
#include "skin_tokens.h"

namespace dyn {

// The MD3 baseline purple (#6750A4): the seed used when a cover has no usable
// hue, so "album colour" degrades to the standard scheme instead of to grey.
constexpr uint32_t kBaselineSeed = 0x6750A4;

// ---------------------------------------------------------------- sRGB / OKLab
inline float srgb_to_linear(float c)
{
    return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
}

inline float linear_to_srgb(float c)
{
    c = c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : c);
    return c <= 0.0031308f ? c * 12.92f : 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

struct Rgb { float r, g, b; };     // 0..1, sRGB encoded
struct Lab { float L, a, b; };

inline void linear_to_oklab(float r, float g, float b, Lab& out)
{
    const float l = 0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b;
    const float m = 0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b;
    const float s = 0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b;
    const float l_ = cbrtf(l), m_ = cbrtf(m), s_ = cbrtf(s);
    out.L = 0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_;
    out.a = 1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_;
    out.b = 0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_;
}

inline void oklab_to_linear(const Lab& c, float& r, float& g, float& b)
{
    const float l_ = c.L + 0.3963377774f * c.a + 0.2158037573f * c.b;
    const float m_ = c.L - 0.1055613458f * c.a - 0.0638541728f * c.b;
    const float s_ = c.L - 0.0894841775f * c.a - 1.2914855480f * c.b;
    const float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
    r = +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    b = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
}

inline bool in_gamut(const Lab& c, float slack = 0.0005f)
{
    float r, g, b;
    oklab_to_linear(c, r, g, b);
    return r >= -slack && r <= 1.0f + slack &&
           g >= -slack && g <= 1.0f + slack &&
           b >= -slack && b <= 1.0f + slack;
}

inline uint32_t to_rgb32(const Lab& c)
{
    // Reduce chroma until it fits the sRGB gamut, then quantise.
    Lab p = c;
    if (!in_gamut(p)) {
        float lo = 0.0f, hi = 1.0f;
        for (int i = 0; i < 18; i++) {
            const float mid = 0.5f * (lo + hi);
            Lab t = c;
            t.a *= mid;
            t.b *= mid;
            if (in_gamut(t)) lo = mid; else hi = mid;
        }
        p.a *= lo;
        p.b *= lo;
    }
    float r, g, b;
    oklab_to_linear(p, r, g, b);
    const int R = (int)(linear_to_srgb(r) * 255.0f + 0.5f);
    const int G = (int)(linear_to_srgb(g) * 255.0f + 0.5f);
    const int B = (int)(linear_to_srgb(b) * 255.0f + 0.5f);
    return ((uint32_t)(R < 0 ? 0 : (R > 255 ? 255 : R)) << 16) |
           ((uint32_t)(G < 0 ? 0 : (G > 255 ? 255 : G)) << 8) |
           (uint32_t)(B < 0 ? 0 : (B > 255 ? 255 : B));
}

inline void unpack(uint32_t rgb, Rgb& out)
{
    out.r = (float)((rgb >> 16) & 0xFF) / 255.0f;
    out.g = (float)((rgb >> 8) & 0xFF) / 255.0f;
    out.b = (float)(rgb & 0xFF) / 255.0f;
}

inline Lab from_rgb32(uint32_t rgb)
{
    Rgb c;
    unpack(rgb, c);
    Lab out;
    linear_to_oklab(srgb_to_linear(c.r), srgb_to_linear(c.g), srgb_to_linear(c.b), out);
    return out;
}

// --------------------------------------------------------------- tone <-> L
// Material tone is CIE L* (0..100). OKLab L for a neutral of that luminance is
// cbrt(Y), since the OKLab L row sums to 1 and a neutral has l = m = s = Y.
inline float ok_L_for_tone(float tone)
{
    const float y = tone > 8.0f ? powf((tone + 16.0f) / 116.0f, 3.0f)
                                : tone / 903.3f;
    return cbrtf(y < 0.0f ? 0.0f : y);
}

// Chroma the sRGB gamut can hold at this tone, as a fraction of full chroma:
// 0 at black and white, 1 in the mid tones. Saturated colour is impossible at
// the ends, so every palette tapers there instead of clipping to a flat band.
inline float chroma_budget(float tone)
{
    const float x = sinf(3.14159265f * tone / 100.0f);
    return powf(x < 0.0f ? 0.0f : x, 0.85f);
}

struct Hcl { float hue; float chroma; };   // hue in radians, chroma 0..~0.37

inline Hcl hcl_of(uint32_t rgb)
{
    const Lab c = from_rgb32(rgb);
    Hcl out;
    out.hue = atan2f(c.b, c.a);
    out.chroma = sqrtf(c.a * c.a + c.b * c.b);
    return out;
}

inline uint32_t tone_of(const Hcl& p, float chroma_scale, float tone)
{
    const float c = p.chroma * chroma_scale * chroma_budget(tone);
    Lab l;
    l.L = ok_L_for_tone(tone);
    l.a = c * cosf(p.hue);
    l.b = c * sinf(p.hue);
    return to_rgb32(l);
}

// ------------------------------------------------------------------- schemes
// The role -> tone table from the MD3 spec (light / dark), same tones the
// shipped baseline scheme uses.
inline SkinTokens scheme_md3(uint32_t seed, bool light)
{
    const Hcl p = hcl_of(seed);
    // Secondary keeps the hue at low chroma; tertiary shifts the hue for the
    // contrasting accent. Neutrals are the same hue with almost no chroma, which
    // is what makes "tinted grey" surfaces read as part of the same scheme.
    const Hcl secondary = { p.hue, p.chroma };
    const float tertiary_hue = p.hue + 1.04719755f;   // +60 deg
    const Hcl tertiary = { tertiary_hue, p.chroma };

    const float primary_scale = 1.0f;
    const float secondary_scale = 0.24f;   // secondary is a muted accent
    const float tertiary_scale = 0.62f;
    const float neutral_scale = 0.085f;
    const float variant_scale = 0.10f;

    auto pm = [&](float tone) { return tone_of(p, primary_scale, tone); };
    auto sec = [&](float tone) { return tone_of(secondary, secondary_scale, tone); };
    auto ter = [&](float tone) { return tone_of(tertiary, tertiary_scale, tone); };
    auto neu = [&](float tone) { return tone_of(p, neutral_scale, tone); };
    auto var = [&](float tone) { return tone_of(p, variant_scale, tone); };

    SkinTokens t = md3_skin;   // typography, shadows, geometry stay as they are

    if (light) {
        t.surface = neu(94);   // surface-container: this token is the card fill
        t.surface_high = neu(94);
        t.surface_highest = neu(90);
        t.on_surface = neu(10);
        t.on_surface_variant = var(30);
        t.primary = pm(40);
        t.on_button_active = pm(100);   // on-primary
        t.tertiary = ter(40);
    } else {
        t.surface = neu(12);   // surface-container: this token is the card fill
        t.surface_high = neu(17);
        t.surface_highest = neu(22);
        t.on_surface = neu(90);
        t.on_surface_variant = var(80);
        t.primary = pm(80);
        t.on_button_active = pm(20);    // on-primary
        t.tertiary = ter(80);
    }
    // Accent-derived fills keep working as state layers on top of a surface.
    t.button_fill = t.on_surface_variant;
    t.button_fill_alpha = 0.12f;
    t.button_fill_active = t.primary;
    t.button_fill_active_alpha = 1.0f;
    // MD3 progress track is secondary-container, not a surface tone.
    t.progress_track = sec(light ? 90.0f : 30.0f);
    t.progress_track_alpha = 1.0f;
    return t;
}

// The Mist material, tinted by the cover. Same structure as the static Mist
// tokens (near-opaque body, 1px inner rim, specular top edge, squircle); only
// the hues come from the seed. Two chroma budgets: the panel takes a real but
// restrained tint, the text a trace, because the highlight has to stay light and
// the text has to stay neutral for contrast.
//
// Note the tone choice: a tone 99 panel would be neutral no matter the seed -
// the gamut simply has no room for chroma that close to white - so the tinted
// panel sits at 96-99 (light) / 12-17 (dark) where the bell curve still leaves
// room for a visible hue.
inline SkinTokens scheme_mist(uint32_t seed, bool light)
{
    const SkinTokens& base = light ? mist_skin_light : mist_skin;
    const Hcl p = hcl_of(seed);
    // A dark panel carries a real tint; a light one is much closer to white,
    // where more chroma would read as coloured paper instead of tinted glass.
    const float panel = light ? 0.55f : 0.75f;
    const float ink = 0.07f;     // text keeps its neutrality
    auto pn = [&](float tone) { return tone_of(p, panel, tone); };
    auto nt = [&](float tone) { return tone_of(p, ink, tone); };

    const SkinTokens accent = scheme_md3(seed, light);
    SkinTokens t = base;

    if (light) {
        t.surface = pn(96);              // Hwnd fallback / opaque fill
        t.surface_high = pn(93);
        t.surface_highest = pn(89);
        t.card_fill_top = pn(97);
        t.card_fill_bottom = pn(94);
        t.on_surface = nt(9);
        t.on_surface_variant = nt(34);
    } else {
        t.surface = pn(14);
        t.surface_high = pn(19);
        t.surface_highest = pn(24);
        t.card_fill_top = pn(18);
        t.card_fill_bottom = pn(12);
        t.on_surface = nt(93);
        t.on_surface_variant = nt(76);
    }

    // Accents come from the full-chroma scheme, so the controls still pop.
    t.primary = accent.primary;
    t.tertiary = accent.tertiary;
    t.on_button_active = t.primary;      // tinted fill: the glyph keeps the accent
    // The specular highlight stays neutral white: it is reflected light.
    return t;
}

// ------------------------------------------------------- album seed extraction
// A small "what colour is this cover" pass: quantise to 24 hue bins, ignore
// near-grey and near-black/white pixels (they carry no usable hue), then take
// the bin with the most chroma mass. Averaging the hue circularly avoids the
// red-wrap problem.
inline uint32_t seed_from_bgra(const uint8_t* px, int w, int h, int stride,
                               uint32_t fallback)
{
    if (!px || w <= 1 || h <= 1) return fallback;

    const int kBins = 24;
    float weight[kBins] = {};
    float sa[kBins] = {}, sb[kBins] = {};

    const int step = (w * h > 40000) ? 4 : 1;   // big covers: sample every 4th px
    for (int y = 0; y < h; y += step) {
        const uint8_t* row = px + (size_t)y * stride;
        for (int x = 0; x < w; x += step) {
            const uint8_t* p = row + (size_t)x * 4;
            const float b = (float)p[0] / 255.0f;
            const float g = (float)p[1] / 255.0f;
            const float r = (float)p[2] / 255.0f;
            const float a = (float)p[3] / 255.0f;
            if (a < 0.5f) continue;

            Lab c;
            linear_to_oklab(srgb_to_linear(r), srgb_to_linear(g), srgb_to_linear(b), c);
            const float chroma = sqrtf(c.a * c.a + c.b * c.b);
            if (chroma < 0.035f) continue;            // grey: no hue to take
            if (c.L < 0.12f || c.L > 0.90f) continue; // too close to black/white

            int bin = (int)((atan2f(c.b, c.a) + 3.14159265f) / (2.0f * 3.14159265f) * kBins);
            bin = bin < 0 ? 0 : (bin >= kBins ? kBins - 1 : bin);
            const float wgt = chroma * chroma;        // favour vivid pixels
            weight[bin] += wgt;
            sa[bin] += c.a * wgt;
            sb[bin] += c.b * wgt;
        }
    }

    int best = -1;
    float best_w = 0.0f;
    for (int i = 0; i < kBins; i++) {
        if (weight[i] > best_w) { best_w = weight[i]; best = i; }
    }
    if (best < 0 || best_w <= 0.0f) return fallback;

    // Refine against the two neighbouring bins so a hue on a bin edge does not
    // jump by 15 degrees.
    float wa = 0.0f, wb = 0.0f, wt = 0.0f;
    for (int d = -1; d <= 1; d++) {
        const int i = (best + d + kBins) % kBins;
        wa += sa[i]; wb += sb[i]; wt += weight[i];
    }
    if (wt <= 0.0f) return fallback;

    Hcl c;
    c.hue = atan2f(wb, wa);
    c.chroma = sqrtf(wa * wa + wb * wb) / wt;   // weighted mean magnitude
    return tone_of(c, 1.0f, 50.0f);             // a mid-tone swatch of that hue
}

} // namespace dyn

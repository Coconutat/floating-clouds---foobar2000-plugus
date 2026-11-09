#pragma once

#include "config.h"
#include "md3_theme.h"
#include "skin_tokens.h"

// ============================================================================
// skin_theme.h - token lookup, keyed by (skin, light).
// The token values themselves live in skin_tokens.h (no foobar2000 dependency);
// the painting that consumes them lives in styles/glass_card.h.
// ============================================================================

inline const SkinTokens& get_skin_tokens(FloatingSkin skin, bool light)
{
    // Liquid Glass is one material, not a light/dark pair: its ink polarity is
    // decided by the backdrop it measured (adaptive_ink), so the appearance
    // setting must not change it. Ignoring `light` here is what makes the skin
    // be the same glass whatever the colour mode says.
    if (skin == FloatingSkin::LiquidGlass) return liquid_glass_skin;

    if (light) {
        return (skin == FloatingSkin::Mist) ? mist_skin_light : md3_skin_light;
    }
    return (skin == FloatingSkin::Mist) ? mist_skin : md3_skin;
}

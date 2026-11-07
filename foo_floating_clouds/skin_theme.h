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
    if (light) {
        return (skin == FloatingSkin::Apple) ? apple_skin_light : md3_skin_light;
    }
    return (skin == FloatingSkin::Apple) ? apple_skin : md3_skin;
}

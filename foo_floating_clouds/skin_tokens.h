#pragma once

#include <cstdint>
#include "md3_theme.h"

// ============================================================================
// SkinTokens - per-skin visual tokens shared by every render primitive.
// FloatingSkin (layout-independent material system) x FloatingStyle (layout).
//
// This header deliberately has NO foobar2000 dependency: styles/glass_card.h
// (the actual painter) includes it, and a standalone probe renders the same
// pixels without linking against the SDK.
// ============================================================================

struct SkinTokens {
    // Opaque / fallback surfaces
    uint32_t surface;            // Hwnd fallback fill; MD3 card fill
    uint32_t surface_high;       // album-art placeholder
    uint32_t surface_highest;    // playlist scrollbar thumb / search field

    // Content
    uint32_t on_surface;
    uint32_t on_surface_variant;

    // Accents
    uint32_t primary;
    uint32_t tertiary;
    uint32_t error;

    // Control-button fills (draw_button): inactive / active (pressed uses a
    // state layer on top, see draw_button).
    uint32_t button_fill;        float button_fill_alpha;
    uint32_t button_fill_active; float button_fill_active_alpha;

    // Progress bar/ring track.
    uint32_t progress_track; float progress_track_alpha;

    // ---- Card material -----------------------------------------------------
    // glass = layered frosted material (Mist). false = the pre-existing opaque
    // MD3 card, which must keep looking exactly as it did.
    bool glass;
    // superellipse = continuous corner ("squircle"). false = a plain
    // circular-arc corner (MD3, and the rounded rects the rest of the UI uses).
    bool superellipse;

    // Body fill: a shallow vertical gradient (lit top, deeper bottom). Both
    // stops are near-opaque on purpose: UpdateLayeredWindow cannot blur the
    // desktop behind the window, so a low-alpha fill would let sharp desktop
    // detail read straight through the card. The effective transparency is
    // (alpha x the opacity setting); at the default 220 it lands at ~85%,
    // which reads as a frosted panel rather than a clear pane.
    uint32_t card_fill_top;    float card_fill_top_alpha;
    uint32_t card_fill_bottom; float card_fill_bottom_alpha;

    // Inner 1px rim, drawn INSIDE the silhouette (never as a path stroke, whose
    // outer half would sit on transparent pixels and read as a black/white
    // outline around the whole card). Bright at the top edge, shaded at the
    // bottom edge, fully transparent at mid height.
    uint32_t card_rim_light;   float card_rim_light_alpha;
    uint32_t card_rim_shade;   float card_rim_shade_alpha;

    // Specular bloom just under the top edge (the light source). The stop is a
    // fraction of the card height, clamped at draw time to 16..44px so a tall
    // surface (the playlist panel) gets a highlight, not a white wash.
    uint32_t card_gloss;       float card_gloss_alpha;  float card_gloss_stop;

    // Inner depth shadow at the bottom edge: the "thickness" of the glass. The
    // stop is the fraction of the height where it starts, clamped at draw time
    // so the band stays 8..30px tall.
    uint32_t card_depth;       float card_depth_alpha;  float card_depth_stop;

    // ---- Shape + elevation -------------------------------------------------
    float corner_card;      // clamped to half the card's shorter side at draw time
    float shadow_inset;     // transparent margin around the card
    // Two shadow layers, both fitted inside shadow_inset because a blur wider
    // than the margin gets clipped into a rectangular ridge around the card.
    // blur is the box-blur radius (3 passes -> sigma = sqrt(b*b + b)).
    float shadow_alpha;     int shadow_blur;           // ambient layer
    float shadow_contact_alpha; int shadow_contact_blur; // contact layer
    float shadow_offset_y;  // downward offset: elevation, not an outline

    // State layers
    float hover_state;
    float pressed_state;

    // Type ramp (Segoe UI; weights fixed: title SemiBold, artist/small Normal)
    float title_size;
    float artist_size;
    float small_size;
};

// MD3 skin (dark): exactly the values shipped before the material rework. Only
// the elevation shadow changed, and only to fit inside shadow_inset.
constexpr SkinTokens md3_skin = {
    md3::surface_container,          // surface
    md3::surface_container_high,     // surface_high
    md3::surface_container_highest,  // surface_highest
    md3::on_surface,                 // on_surface
    md3::on_surface_variant,         // on_surface_variant
    md3::primary,                    // primary
    md3::tertiary,                   // tertiary
    md3::error,                      // error
    md3::on_surface_variant, 0.12f,  // button_fill (inactive)
    md3::primary, 0.20f,             // button_fill_active
    md3::on_surface_variant, 0.25f,  // progress_track
    false,                           // glass (opaque card)
    false,                           // superellipse (circular corner)
    md3::surface_container, 1.0f,    // card_fill_top
    md3::surface_container, 1.0f,    // card_fill_bottom
    0x000000, 0.0f,                  // card_rim_light (unused)
    0x000000, 0.0f,                  // card_rim_shade (unused)
    0xFFFFFF, 0.0f, 0.0f,            // card_gloss (unused)
    0x000000, 0.0f, 0.0f,            // card_depth (unused)
    16.0f,                           // corner_card
    8.0f,                            // shadow_inset
    0.30f, 3,                        // ambient shadow (alpha, blur)
    0.00f, 0,                        // contact shadow (disabled)
    2.0f,                            // shadow_offset_y
    md3::hover_state,                // hover_state
    md3::pressed_state,              // pressed_state
    14.0f,                           // title_size
    11.0f,                           // artist_size
    10.0f,                           // small_size
};

// MD3 light skin: Material 3 baseline LIGHT scheme, same token source
// (SDK/MD3/material-web/tokens/versions/v0_192/_md-sys-color.scss values-light).
constexpr SkinTokens md3_skin_light = {
    0xF3EDF7,                        // surface (neutral94 surface-container)
    0xECE6F0,                        // surface_high (neutral92)
    0xE6E0E9,                        // surface_highest (neutral90)
    0x1D1B20,                        // on_surface (neutral10)
    0x49454F,                        // on_surface_variant (neutral-variant30)
    0x6750A4,                        // primary (primary40)
    0x7D5260,                        // tertiary (tertiary40)
    0xB3261E,                        // error (error40)
    0x49454F, 0.12f,                 // button_fill (inactive)
    0x6750A4, 0.20f,                 // button_fill_active
    0x49454F, 0.25f,                 // progress_track
    false,                           // glass (opaque card)
    false,                           // superellipse (circular corner)
    0xF3EDF7, 1.0f,                  // card_fill_top
    0xF3EDF7, 1.0f,                  // card_fill_bottom
    0x000000, 0.0f,                  // card_rim_light (unused)
    0x000000, 0.0f,                  // card_rim_shade (unused)
    0xFFFFFF, 0.0f, 0.0f,            // card_gloss (unused)
    0x000000, 0.0f, 0.0f,            // card_depth (unused)
    16.0f,                           // corner_card
    8.0f,                            // shadow_inset
    0.20f, 3,                        // ambient shadow (lighter in light mode)
    0.00f, 0,                        // contact shadow (disabled)
    2.0f,                            // shadow_offset_y
    md3::hover_state,                // hover_state
    md3::pressed_state,              // pressed_state
    14.0f,                           // title_size
    11.0f,                           // artist_size
    10.0f,                           // small_size
};

// Mist (dark): a frosted floating panel. It deliberately does not attempt a
// real backdrop-blur glass: a layered window cannot sample what is behind
// it, so that look is not reachable here and claiming it would be
// dishonest. What it is:
// a near-opaque cool-neutral panel with a 1px inner rim, a specular top edge
// and continuous corners.
// - Body: near-opaque cool-neutral dark (#26262C -> #141417). The old fill was
//   0x3A3A40 -> 0x1C1C1E at 72/85%, which over any desktop read as a black
//   slab with a milky blue-grey top band.
// - Rim: white 30% at the top edge only, black 35% at the bottom edge; nothing
//   at mid height, so there is no outline around the card.
// - palette: iOS system blue #0A84FF, system pink #FF375F.
constexpr SkinTokens mist_skin = {
    0x1C1C1E,                        // surface (dark elevated, Hwnd fallback)
    0x2C2C2E,                        // surface_high
    0x3A3A3C,                        // surface_highest
    0xF2F3F7,                        // on_surface
    0xA8A8B0,                        // on_surface_variant
    0x0A84FF,                        // primary (system blue, dark)
    0xFF375F,                        // tertiary (system pink, dark)
    0xFF453A,                        // error (iOS systemRed dark)
    0xFFFFFF, 0.16f,                 // button_fill (frosted white circle)
    0x0A84FF, 0.28f,                 // button_fill_active (vibrant blue fill)
    0xFFFFFF, 0.20f,                 // progress_track
    true,                            // glass
    true,                            // superellipse
    0x26262C, 0.94f,                 // card_fill_top
    0x141417, 0.97f,                 // card_fill_bottom
    0xFFFFFF, 0.30f,                 // card_rim_light (top highlight)
    0x000000, 0.35f,                 // card_rim_shade (bottom shade)
    0xFFFFFF, 0.12f, 0.32f,          // card_gloss (soft top bloom)
    0x000000, 0.10f, 0.86f,          // card_depth (bottom thickness)
    26.0f,                           // corner_card
    8.0f,                            // shadow_inset
    0.22f, 3,                        // ambient shadow (fits the 8px margin)
    0.12f, 1,                        // contact shadow
    2.0f,                            // shadow_offset_y
    0.08f,                           // hover_state
    0.12f,                           // pressed_state
    15.0f,                           // title_size
    12.0f,                           // artist_size
    10.0f,                           // small_size
};

// Mist (light): the light counterpart of the same material. Bright, cool,
// near-opaque panel with dark text; the rim highlight is invisible over a white
// desktop and catches on dark content, the bottom shade gives the card an edge.
constexpr SkinTokens mist_skin_light = {
    0xF2F2F7,                        // surface (light elevated, Hwnd fallback)
    0xE9E9EE,                        // surface_high
    0xE0E0E5,                        // surface_highest
    0x131316,                        // on_surface
    0x4E4E58,                        // on_surface_variant
    0x007AFF,                        // primary (system blue, light)
    0xFF2D55,                        // tertiary (system pink, light)
    0xFF3B30,                        // error (iOS systemRed light)
    0x000000, 0.07f,                 // button_fill
    0x007AFF, 0.18f,                 // button_fill_active
    0x000000, 0.13f,                 // progress_track
    true,                            // glass
    true,                            // superellipse
    0xFFFFFF, 0.95f,                 // card_fill_top (near-white; a tint would not
    0xF2F5FA, 0.98f,                 // card_fill_bottom   separate better: the card
                                     // sits on its own shadow, which costs ~7 levels)
    0xFFFFFF, 0.95f,                 // card_rim_light (top highlight)
    0x000000, 0.07f,                 // card_rim_shade (bottom shade)
    0xFFFFFF, 0.42f, 0.28f,          // card_gloss
    0x000000, 0.045f, 0.88f,         // card_depth
    26.0f,                           // corner_card
    8.0f,                            // shadow_inset
    0.16f, 3,                        // ambient shadow
    0.10f, 1,                        // contact shadow
    2.0f,                            // shadow_offset_y
    0.08f,                           // hover_state
    0.12f,                           // pressed_state
    15.0f,                           // title_size
    12.0f,                           // artist_size
    10.0f,                           // small_size
};

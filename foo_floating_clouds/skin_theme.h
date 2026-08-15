#pragma once

#include "config.h"
#include "md3_theme.h"

// ============================================================================
// SkinTokens - per-skin visual tokens shared by every render primitive.
// FloatingSkin (layout-independent material system) x FloatingStyle (layout).
// ============================================================================

struct SkinTokens {
    // Opaque / fallback surfaces
    uint32_t surface;            // Hwnd fallback fill; MD3 ULW card fill
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

    // ULW glass card. The fill is a vertical gradient (top light -> bottom
    // slightly darker) instead of a flat tint: a flat dark tint reads as
    // "black transparent glass", while the gradient reads as a lit material.
    uint32_t glass_fill_top;    float glass_fill_top_alpha;
    uint32_t glass_fill_bottom; float glass_fill_bottom_alpha;

    // Glass rim: a hairline stroke whose brightness falls off toward the
    // bottom (edge refraction fake). Top is the light-catching edge.
    uint32_t glass_stroke_top;    float glass_stroke_top_alpha;
    uint32_t glass_stroke_bottom; float glass_stroke_bottom_alpha;
    float glass_stroke_width;

    // Specular bloom just below the top edge, fading out by specular_stop.
    uint32_t glass_specular; float glass_specular_alpha; float glass_specular_stop;

    // Shape + shadow. Two shadow layers: a tight contact shadow and a wide
    // soft ambient shadow (contact alpha 0 disables that layer).
    float corner_card;      // clamped to half the card's shorter side at draw time
    float shadow_inset;     // transparent margin around the card (both skins: 8)
    float shadow_alpha;     int shadow_blur;          // ambient layer
    float shadow_contact_alpha; int shadow_contact_blur; // contact layer

    // State layers
    float hover_state;
    float pressed_state;

    // Type ramp (Segoe UI; weights fixed: title SemiBold, artist/small Normal)
    float title_size;
    float artist_size;
    float small_size;
};

// MD3 skin: exactly the values shipped before this change (md3_theme.h + config.h).
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
    md3::surface_container, 1.0f,    // glass_fill_top (opaque card)
    md3::surface_container, 1.0f,    // glass_fill_bottom
    0xFFFFFF, 0.0f,                  // glass_stroke_top (none)
    0xFFFFFF, 0.0f,                  // glass_stroke_bottom
    1.0f,                            // glass_stroke_width
    0xFFFFFF, 0.0f, 0.0f,            // glass_specular (none)
    16.0f,                           // corner_card
    8.0f,                            // shadow_inset
    0.40f, 5,                        // ambient shadow (alpha, blur)
    0.0f, 0,                         // contact shadow (disabled)
    md3::hover_state,                // hover_state
    md3::pressed_state,              // pressed_state
    14.0f,                           // title_size
    11.0f,                           // artist_size
    10.0f,                           // small_size
};

// Apple skin: frosted dark liquid-glass approximation + iOS system colors.
// - Fill is a subtle vertical gradient of COOL DARK GRAY at HIGH opacity:
//   Apple's DARK-mode glass is a dark neutral material, not black and not
//   light gray. Keeping it dark preserves the white text legibility that a
//   light frosted fill destroys (white text on light glass = invisible).
// - The "glass" comes from the specular top edge, the gradient hairline rim,
//   and the edge halo — not from a light wash over the whole card.
// - Buttons are frosted white circles; active buttons use a vibrant blue fill.
// - Two-layer shadow: tight contact + wide soft ambient, like Apple's
//   elevation.
// - systemBlue dark #0A84FF; systemPink #FF375F for the visualizer ramp tail.
// - 24px card corner (circular arc, D2D rounded-rect approximation of a
//   continuous corner).
constexpr SkinTokens apple_skin = {
    0x1C1C1E,                        // surface (Apple dark elevated, Hwnd fallback)
    0x2C2C2E,                        // surface_high
    0x3A3A3C,                        // surface_highest
    0xF5F5F7,                        // on_surface (white text, dark material)
    0x98989D,                        // on_surface_variant (Apple secondary label)
    0x0A84FF,                        // primary (iOS systemBlue dark)
    0xFF375F,                        // tertiary (iOS systemPink)
    0xFF453A,                        // error (iOS systemRed dark)
    0xFFFFFF, 0.14f,                 // button_fill (frosted white circle)
    0x0A84FF, 0.22f,                 // button_fill_active (vibrant blue fill)
    0xFFFFFF, 0.18f,                 // progress_track (frosted white track)
    0x3A3A40, 0.72f,                 // glass_fill_top (cool dark gray, slightly lifted)
    0x1C1C1E, 0.85f,                 // glass_fill_bottom (dark neutral, near-opaque)
    0xFFFFFF, 0.55f,                 // glass_stroke_top (bright rim)
    0xFFFFFF, 0.12f,                 // glass_stroke_bottom (faint rim)
    1.5f,                            // glass_stroke_width
    0xFFFFFF, 0.18f, 0.22f,          // glass_specular (tight top-edge bloom)
    24.0f,                           // corner_card
    8.0f,                            // shadow_inset (kept equal to MD3 to avoid geometry churn)
    0.18f, 10,                       // ambient shadow (wide, soft)
    0.18f, 2,                        // contact shadow (tight)
    0.07f,                           // hover_state
    0.12f,                           // pressed_state
    15.0f,                           // title_size
    12.0f,                           // artist_size
    10.0f,                           // small_size
};

inline const SkinTokens& get_skin_tokens(FloatingSkin skin)
{
    return (skin == FloatingSkin::Apple) ? apple_skin : md3_skin;
}

#pragma once

#include <cstdint>

// ============================================================================
// MD3 (Material 3) design tokens — baseline DARK scheme, tokens v0.192
// Source: SDK/MD3/material-web/tokens/versions/v0_192/
//   _md-ref-palette.scss (hex) + _md-sys-color.scss (values-dark role mapping)
// ============================================================================
namespace md3 {

    // --- surfaces -----------------------------------------------------------
    constexpr uint32_t surface                   = 0x141218; // neutral6
    constexpr uint32_t surface_container_lowest  = 0x0F0D13; // neutral4
    constexpr uint32_t surface_container_low     = 0x1D1B20; // neutral10
    constexpr uint32_t surface_container         = 0x211F26; // neutral12
    constexpr uint32_t surface_container_high    = 0x2B2930; // neutral17
    constexpr uint32_t surface_container_highest = 0x36343B; // neutral22

    // --- content ------------------------------------------------------------
    constexpr uint32_t on_surface         = 0xE6E0E9; // neutral90
    constexpr uint32_t on_surface_variant = 0xCAC4D0; // neutral-variant80
    constexpr uint32_t surface_variant    = 0x49454F; // neutral-variant30
    constexpr uint32_t outline            = 0x938F99; // neutral-variant60
    constexpr uint32_t outline_variant    = 0x49454F; // neutral-variant30

    // --- accent -------------------------------------------------------------
    constexpr uint32_t primary              = 0xD0BCFF; // primary80
    constexpr uint32_t on_primary           = 0x381E72; // primary20
    constexpr uint32_t primary_container    = 0x4F378B; // primary30
    constexpr uint32_t on_primary_container = 0xEADDFF; // primary90
    constexpr uint32_t secondary            = 0xCCC2DC; // secondary80
    constexpr uint32_t tertiary             = 0xEFB8C8; // tertiary80
    constexpr uint32_t error                = 0xF2B8B5; // error80

    // --- shape (md-sys-shape) ----------------------------------------------
    constexpr float corner_small  = 8.0f;
    constexpr float corner_medium = 12.0f;
    constexpr float corner_large  = 16.0f;
    constexpr float corner_full   = 9999.0f;

    // --- state layers (md-sys-state) ---------------------------------------
    constexpr float hover_state   = 0.08f;
    constexpr float focus_state   = 0.12f;
    constexpr float pressed_state = 0.12f;
    constexpr float dragged_state = 0.16f;
}

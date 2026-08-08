#pragma once

#include <stdafx.h>

// ============================================================================
// Playlist Organizer - configuration definitions
// ============================================================================

namespace cfg_guids {
    // UI language (0 = English, 1 = Chinese)
    static const GUID language = { 0x7c3d2b1a, 0x4e5f, 0x6071, { 0x82, 0x93, 0xa4, 0xb5, 0xc6, 0xd7, 0xe8, 0x11 } };
}

constexpr int32_t DEFAULT_LANGUAGE = 0;

// --- Organizer constants (destination playlist names are DATA, not UI:
// fixed English names regardless of the interface language; they stay stable
// across runs so rebuild semantics work) ---
inline constexpr const char* kSoundtrackPlaylist   = "Soundtrack";
inline constexpr const char* kVariousArtistsPlaylist = "Various Artists";
inline constexpr const char* kUnknownArtistPlaylist = "Unknown Artist";
// Album name contains this keyword (case-insensitive) -> Soundtrack playlist.
inline constexpr const char* kSoundtrackKeyword    = "soundtrack";

// Case-insensitive "less" for A-Z ordering of playlist names.
inline bool str_less_ci(const pfc::string8& a, const pfc::string8& b)
{
    const char* pa = a.get_ptr();
    const char* pb = b.get_ptr();
    const size_t la = a.length(), lb = b.length();
    const size_t n = (std::min)(la, lb);
    for (size_t i = 0; i < n; i++) {
        char x = pa[i], y = pb[i];
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return x < y;
    }
    return la < lb;
}

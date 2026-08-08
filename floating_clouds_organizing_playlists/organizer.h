#pragma once

#include "stdafx.h"

// ============================================================================
// Organizer - core playlist-organizing logic (see docs/design.md, CONTEXT.md)
// ============================================================================

// Result of one organize run.
struct OrganizeResult {
    t_size playlists_updated = 0;  // destination playlists created or rebuilt
    t_size tracks_organized = 0;   // tracks placed into destination playlists
};

namespace organizer {

    // Organizes the ACTIVE playlist by album artist into generated destination
    // playlists. Routing priority per track:
    //   1. album name contains "soundtrack" (case-insensitive) -> Soundtrack
    //   2. album artist == "Various Artists" (ci)               -> Various Artists
    //   3. album artist missing -> track artist -> "Unknown Artist"
    //   4. otherwise -> playlist named after the album artist
    // Every track lands in exactly one destination (single attribution).
    // Destinations are rebuilt (clear + refill) each run; empty groups are skipped;
    // source order is preserved within each destination. Destinations are created
    // in A-Z (case-insensitive) order and then LOCKED (foobar2000 playlist lock) so
    // their order and contents are protected between runs. The source playlist is
    // never modified.
    //
    // Returns false and fills p_error on failure (no active playlist, empty source,
    // locked playlist, etc.). Call from the main thread only.
    bool organize_active_playlist(OrganizeResult& out, pfc::string8& error);

    // Reorders the entire playlist list by name (A-Z, case-insensitive).
    // Returns false and fills p_error on failure. Call from the main thread only.
    bool sort_all_playlists(pfc::string8& error);

}

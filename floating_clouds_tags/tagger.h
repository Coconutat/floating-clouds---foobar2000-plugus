#pragma once

#include "stdafx.h"
#include "itunes.h"
#include "config.h" // TagField / FieldAll

// ============================================================================
// Tag write pipeline: (disc, track) matching + async metadb_io_v2 update
// ============================================================================

struct TagOptions {
    uint32_t fields = FieldAll; // bitmask of TagField
    bool overwrite = false;     // true = overwrite existing, false = fill empty only
    bool force_order = false;   // emergency: ignore (disc,track), write in selection order
};

// Applies Apple tags to `selected` tracks by (discNumber, trackNumber) matching.
// Main thread only. Starts metadb_io_v2::update_info_async (async; a progress
// dialog may appear) and shows a "written N / skipped M" popup on completion.
void apply_tags(const metadb_handle_list& selected, const AppleAlbum& album,
                const TagOptions& options, fb2k::hwnd_t parent);

// Converts the selected tracks' EXISTING local tags from Traditional Chinese to
// Simplified Chinese (char-by-char), in place, across every text meta field.
// Main thread only. Starts metadb_io_v2::update_info_async and shows a
// "converted N / skipped M" popup on completion.
void convert_local_tags_to_simplified(const metadb_handle_list& selected, fb2k::hwnd_t parent);

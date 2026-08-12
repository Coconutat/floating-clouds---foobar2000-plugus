#pragma once

#include "stdafx.h"
#include "json.h"

// ============================================================================
// Apple Music / iTunes Lookup API client
// ============================================================================

// One track as returned by the storefront's lookup.
struct AppleTrack {
    int track_number = 0;
    int disc_number = 1;
    pfc::string8 title;
    pfc::string8 artist;
    pfc::string8 album;
    pfc::string8 album_artist;
    pfc::string8 genre;
    pfc::string8 release_date; // ISO, e.g. "2012-10-22T07:00:00Z"
    bool explicit_flag = false;
};

// One fetched album (album-level metadata + its tracks).
struct AppleAlbum {
    bool ok = false;
    pfc::string8 error;          // human-readable error when !ok
    pfc::string8 album_name;
    pfc::string8 album_artist;
    pfc::string8 genre;
    pfc::string8 release_date;
    int track_count = 0;
    int album_id = 0;
    pfc::string8 region;         // storefront code used
    bool not_available = false;    // storefront responded but has no such album
    bool converted_from_hk = false; // fell back to HK + Traditional->Simplified
    pfc::list_t<AppleTrack> tracks;
};

// Parse `https://music.apple.com/{region}/album/{slug}/{albumID}` (or a bare
// numeric ID). Returns true when an album ID was found; fills out_region
// (storefront code, may stay empty for a bare ID) and out_album_id.
bool parse_album_url(const wchar_t* url, pfc::string8& out_region, int& out_album_id);

// Fetch album + track metadata from the iTunes Lookup API.
// WORKER THREAD ONLY (uses abort_callback; may throw on network/HTTP failure).
void fetch_album(int album_id, const char* region, abort_callback& abort, AppleAlbum& out);

// Convert all text fields in `a` from Traditional Chinese to Simplified Chinese (in place).
void to_simplified(AppleAlbum& a);

// High-level fetch: fetch `region`; if it is CN and the album is unavailable there,
// retry the HK storefront and convert its Traditional Chinese tags to Simplified.
// Throws on network/HTTP failure (like fetch_album).
void fetch_album_auto(int album_id, const char* region, abort_callback& abort, AppleAlbum& out);

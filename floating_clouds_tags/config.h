#pragma once

#include <stdafx.h>

// ============================================================================
// Apple Music Tags - configuration definitions
// ============================================================================

namespace cfg_guids {
    // UI language (0 = English, 1 = Chinese)
    static const GUID language          = { 0x8e2d4b6a, 0x3f1c, 0x5a79, { 0x90, 0x81, 0x72, 0x63, 0x54, 0x45, 0x36, 0x11 } };
    // Default region (index into kRegions)
    static const GUID default_region    = { 0x8e2d4b6a, 0x3f1c, 0x5a79, { 0x90, 0x81, 0x72, 0x63, 0x54, 0x45, 0x36, 0x12 } };
    // Overwrite existing tags by default
    static const GUID overwrite_default = { 0x8e2d4b6a, 0x3f1c, 0x5a79, { 0x90, 0x81, 0x72, 0x63, 0x54, 0x45, 0x36, 0x13 } };
}

constexpr int32_t DEFAULT_LANGUAGE = 0;
constexpr int32_t DEFAULT_REGION   = 0; // CN
constexpr bool    DEFAULT_OVERWRITE = false;

// --- Region table: storefront code + metadata language + label ---
// The storefront decides which catalog (and therefore which metadata language)
// the lookup API reads, so this table doubles as the metadata-language switch.
struct RegionInfo {
    const char* code;        // iTunes country param / URL region segment
    const char* lang;        // metadata language family: zh-Hans, zh-Hant, en, ja, ko
    const wchar_t* label;    // English UI
    const wchar_t* label_zh; // Chinese UI
};

// Append new storefronts at the END: cfg_guids::default_region stores an index
// into this table, so reordering it would silently change everyone's selection.
inline const RegionInfo kRegions[] = {
    { "cn", "zh-Hans", L"CN (简体中文)",   L"CN（简体中文）" },
    { "hk", "zh-Hant", L"HK (繁體中文)",   L"HK（繁體中文）" },
    { "tw", "zh-Hant", L"TW (繁體中文)",   L"TW（繁體中文）" },
    { "jp", "ja",      L"JP (日本語)",     L"JP（日本語）" },
    { "us", "en",      L"US (English)",    L"US（English）" },
    { "gb", "en",      L"GB (English)",    L"GB（English）" },
    { "kr", "ko",      L"KR (한국어)",     L"KR（한국어）" },
    { "sg", "en",      L"SG (English)",    L"SG（English）" },
};
constexpr int32_t kRegionCount = (int32_t)(sizeof(kRegions) / sizeof(kRegions[0]));

// First storefront in kRegions order whose metadata language is `lang`, or -1.
inline int region_index_for_language(const char* lang)
{
    if (!lang || !*lang) return -1;
    for (int i = 0; i < kRegionCount; i++) {
        if (pfc::stricmp_ascii(kRegions[i].lang, lang) == 0) return i;
    }
    return -1;
}

// --- Tag fields (bitmask) ---
enum TagField : uint32_t {
    FieldTitle       = 1u << 0,
    FieldAlbum       = 1u << 1,
    FieldArtist      = 1u << 2,
    FieldAlbumArtist = 1u << 3,
    FieldGenre       = 1u << 4,
    FieldDate        = 1u << 5,
    FieldTrackNo     = 1u << 6,
    FieldDiscNo      = 1u << 7,
    FieldExplicit    = 1u << 8,
    FieldComposer    = 1u << 9,
    FieldCopyright   = 1u << 10,
    FieldTotalTracks = 1u << 11,
    FieldTotalDiscs  = 1u << 12,
    FieldAll         = FieldTitle | FieldAlbum | FieldArtist | FieldAlbumArtist | FieldGenre | FieldDate | FieldTrackNo | FieldDiscNo
                     | FieldComposer | FieldCopyright | FieldTotalTracks | FieldTotalDiscs,
};

// Which tags the tagger writes. Order matches the dialog checkboxes (see dialog.cpp).
inline const uint32_t kWriteFields[] = {
    FieldTitle, FieldAlbum, FieldArtist, FieldAlbumArtist, FieldGenre,
    FieldDate, FieldTrackNo, FieldDiscNo, FieldExplicit,
    FieldComposer, FieldCopyright, FieldTotalTracks, FieldTotalDiscs,
};
constexpr int32_t kWriteFieldCount = (int32_t)(sizeof(kWriteFields) / sizeof(kWriteFields[0]));

#include "stdafx.h"

// ============================================================================
// Playlist Organizer - Component entry point
// ============================================================================

DECLARE_COMPONENT_VERSION(
    "Playlist Organizer",
    "0.1.0",
    "Organizes a foobar2000 playlist by album artist into per-artist playlists, with Soundtrack / Various Artists / Unknown Artist handling. By Coconutat."
);

VALIDATE_COMPONENT_FILENAME("foo_playlist_organizer.dll");
FOOBAR2000_IMPLEMENT_CFG_VAR_DOWNGRADE;

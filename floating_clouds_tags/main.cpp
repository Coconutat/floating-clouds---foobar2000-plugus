#include "stdafx.h"

// ============================================================================
// Apple Music Tags - Component entry point
// ============================================================================

DECLARE_COMPONENT_VERSION(
    "Apple Music Tags",
    "0.1.2",
    "Fetches Apple Music album tags from the iTunes Lookup API and writes them to the selected tracks, per storefront region (metadata language). By Coconutat."
);

VALIDATE_COMPONENT_FILENAME("foo_floating_clouds_tags.dll");
FOOBAR2000_IMPLEMENT_CFG_VAR_DOWNGRADE;

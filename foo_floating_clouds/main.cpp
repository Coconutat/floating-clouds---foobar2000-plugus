#include "stdafx.h"
#include "config.h"

// ============================================================================
// Floating Clouds - Component entry point
// ============================================================================

DECLARE_COMPONENT_VERSION(
    "Floating Clouds",
    "0.1.8",
    "A floating UI overlay for foobar2000 - shows now playing info on desktop or over games. Plugus by Coconutat."
);

VALIDATE_COMPONENT_FILENAME("foo_floating_clouds.dll");
FOOBAR2000_IMPLEMENT_CFG_VAR_DOWNGRADE;
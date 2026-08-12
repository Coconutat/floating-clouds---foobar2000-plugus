#include "stdafx.h"
#include "config.h"

// ============================================================================
// Floating Clouds - Component entry point
// ============================================================================

// The plugin DLL's own module handle, captured at DLL load. Component resources
// (e.g. the tray icon) must load from THIS instance — not GetModuleHandle(NULL),
// which returns the HOST executable (foobar2000.exe), so loading the icon from
// it would silently fail and leave a blank tray icon.
HINSTANCE g_hInstance;

extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) g_hInstance = hInstance;
    return TRUE;
}

DECLARE_COMPONENT_VERSION(
    "Floating Clouds",
    "0.1.8",
    "A floating UI overlay for foobar2000 - shows now playing info on desktop or over games. Plugus by Coconutat."
);

VALIDATE_COMPONENT_FILENAME("foo_floating_clouds.dll");
FOOBAR2000_IMPLEMENT_CFG_VAR_DOWNGRADE;
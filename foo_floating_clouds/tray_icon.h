#pragma once

#include "stdafx.h"
#include "config.h"

// ============================================================================
// TrayIcon - System tray icon with context menu
// ============================================================================

class FloatingCloudsWindow; // forward decl

class TrayIcon
{
public:
    TrayIcon(FloatingCloudsWindow* window);
    ~TrayIcon();

    bool create(HWND parent_hwnd);
    void destroy();
    void update();

    // Handle WM_TRAYICON message
    LRESULT handle_notify(UINT msg);

private:
    void show_context_menu();
    
    FloatingCloudsWindow* m_window;
    HWND m_parent_hwnd = nullptr;
    bool m_created = false;
    UINT m_taskbar_created_msg = 0;
    
    static constexpr UINT TRAY_ID = 0x9001;
};
#include "stdafx.h"
#include "resource.h"
#include "tray_icon.h"
#include "floating_window.h"
#include "config.h"

// ============================================================================
// TrayIcon implementation
// ============================================================================

TrayIcon::TrayIcon(FloatingCloudsWindow* window)
    : m_window(window)
{
    m_taskbar_created_msg = RegisterWindowMessage(TEXT("TaskbarCreated"));
}

TrayIcon::~TrayIcon()
{
    destroy();
}

bool TrayIcon::create(HWND parent_hwnd)
{
    if (m_created) return true;
    
    m_parent_hwnd = parent_hwnd;
    
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = parent_hwnd;
    nid.uID = TRAY_ID;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = FC_WM_TRAY_NOTIFY;
    nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
    wcscpy_s(nid.szTip, L"Floating Clouds");
    nid.uVersion = NOTIFYICON_VERSION_4;
    
    m_created = Shell_NotifyIconW(NIM_ADD, &nid) == TRUE;
    
    if (m_created) {
        Shell_NotifyIconW(NIM_SETVERSION, &nid);
    }
    
    return m_created;
}

void TrayIcon::destroy()
{
    if (m_created && m_parent_hwnd) {
        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(NOTIFYICONDATAW);
        nid.hWnd = m_parent_hwnd;
        nid.uID = TRAY_ID;
        Shell_NotifyIconW(NIM_DELETE, &nid);
        m_created = false;
    }
}

void TrayIcon::update()
{
    // Could update tooltip text with current track info
}

LRESULT TrayIcon::handle_notify(UINT msg)
{
    // msg is the notification event (WM_RBUTTONUP, WM_LBUTTONUP, etc.)
    switch (msg) {
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            show_context_menu();
            break;
            
        case WM_LBUTTONUP:
            // Toggle window visibility
            m_window->toggle_visibility();
            break;
    }
    return 1;
}

void TrayIcon::show_context_menu()
{
    HMENU menu = CreatePopupMenu();
    
    // Style submenu
    HMENU style_menu = CreatePopupMenu();
    AppendMenu(style_menu, MF_STRING, 1001, L"Mini");
    AppendMenu(style_menu, MF_STRING, 1002, L"Mini Art");
    AppendMenu(style_menu, MF_STRING, 1003, L"Full");
    AppendMenu(style_menu, MF_SEPARATOR, 0, NULL);
    AppendMenu(style_menu, MF_STRING, 1004, L"Minimal Line");
    AppendMenu(style_menu, MF_STRING, 1005, L"Album Focus");
    AppendMenu(style_menu, MF_STRING, 1006, L"Progress Ring");
    AppendMenu(style_menu, MF_STRING, 1007, L"Visualizer");
    AppendMenu(style_menu, MF_STRING, 1008, L"Lyrics Line");
    
    AppendMenu(menu, MF_POPUP, (UINT_PTR)style_menu, L"Style");
    AppendMenu(menu, MF_SEPARATOR, 0, NULL);
    AppendMenu(menu, MF_STRING, 2001, m_window->is_visible() ? L"Hide" : L"Show");
    AppendMenu(menu, MF_STRING, 2002, L"Exit");
    
    // Get cursor position for the menu
    POINT pt;
    GetCursorPos(&pt);
    
    // Show context menu
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, m_parent_hwnd, NULL);
    
    DestroyMenu(style_menu);
    DestroyMenu(menu);
    
    // Handle command
    if (cmd >= 1001 && cmd <= 1008) {
        FloatingStyle style = static_cast<FloatingStyle>(cmd - 1001);
        m_window->set_style(style);
    } else if (cmd == 2001) {
        m_window->toggle_visibility();
    } else if (cmd == 2002) {
        // Post quit message
        PostQuitMessage(0);
    }
}
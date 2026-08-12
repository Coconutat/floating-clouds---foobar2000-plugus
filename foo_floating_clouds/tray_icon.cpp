#include "stdafx.h"
#include "resource.h"
#include "tray_icon.h"
#include "floating_window.h"
#include "config.h"
#include "localization.h"

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
    nid.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON1));
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
    
    // Style submenu (localized)
    HMENU style_menu = CreatePopupMenu();
    for (int i = 0; i < (int)FloatingStyle::Count; i++) {
        AppendMenu(style_menu, MF_STRING, 1001 + i, tr_style(static_cast<FloatingStyle>(i)));
        if (i == 2) AppendMenu(style_menu, MF_SEPARATOR, 0, NULL); // group core vs extended styles
    }
    
    AppendMenu(menu, MF_POPUP, (UINT_PTR)style_menu, tr(L"Style", L"样式"));
    AppendMenu(menu, MF_SEPARATOR, 0, NULL);
    AppendMenu(menu, MF_STRING, 2001, m_window->is_visible() ? tr(L"Hide", L"隐藏") : tr(L"Show", L"显示"));
    AppendMenu(menu, MF_STRING, 2002, tr(L"Exit", L"退出"));
    
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
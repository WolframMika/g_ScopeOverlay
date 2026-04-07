#include "tray.h"
#include <shellapi.h>

#pragma comment(lib, "shell32.lib")

static NOTIFYICONDATA g_nid = {};

bool Tray_Init(HWND hwnd, HINSTANCE hInstance)
{
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize           = sizeof(NOTIFYICONDATA);
    g_nid.hWnd             = hwnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"Overlay App");

    return Shell_NotifyIcon(NIM_ADD, &g_nid) != FALSE;
}

void Tray_Destroy()
{
    Shell_NotifyIcon(NIM_DELETE, &g_nid);
}

void Tray_ShowContextMenu(HWND hwnd)
{
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    AppendMenuW(hMenu, MF_STRING,    ID_TRAY_SETTINGS, L"Settings");
    AppendMenuW(hMenu, MF_SEPARATOR, 0,                nullptr);
    AppendMenuW(hMenu, MF_STRING,    ID_TRAY_EXIT,     L"Exit");

    // Required: set foreground so menu dismisses on click-away
    SetForegroundWindow(hwnd);

    POINT pt = {};
    GetCursorPos(&pt);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);

    DestroyMenu(hMenu);
}

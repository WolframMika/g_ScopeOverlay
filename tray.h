#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Custom message sent by the tray icon to the hidden main window
#define WM_TRAYICON     (WM_USER + 1)

// Menu item IDs
#define ID_TRAY_SETTINGS  1001
#define ID_TRAY_EXIT      1002

/// <summary>Installs the system tray icon. Must be called on the main thread.</summary>
/// <returns>true on success</returns>
bool  Tray_Init(HWND hwnd, HINSTANCE hInstance);

/// <summary>Removes the tray icon. Call before exit.</summary>
void  Tray_Destroy();

/// <summary>Pops up the right-click context menu at the current cursor position.</summary>
void  Tray_ShowContextMenu(HWND hwnd);

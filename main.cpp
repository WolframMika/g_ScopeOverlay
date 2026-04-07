// ============================================================
//  main.cpp
//  Application entry point.
//
//  Responsibilities:
//    1. Exposes g_running (atomic bool) for the overlay thread.
//    2. Starts the overlay thread.
//    3. Creates a hidden "tray host" window for Win32 message routing.
//    4. Installs the system-tray icon.
//    5. Runs the main message + render loop (handles tray events,
//       opens/closes the settings window, calls Settings_RenderFrame()).
//    6. On exit: signals overlay thread, joins it, removes tray icon.
//
//  NOTE: ImGui is NOT used in this file directly.
//        Each window (overlay, settings) owns its own ImGui context.
// ============================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <thread>

#include "tray.h"
#include "overlay.h"
#include "settings.h"

// ============================================================
//  Shared atomic flags
// ============================================================
std::atomic<bool> g_running  { true };  // overlay thread monitors this
std::atomic<bool> g_settingsOpen { false }; // informational (not strictly needed)

// ============================================================
//  Hidden main window (tray host)
// ============================================================
static HINSTANCE g_hInstance = nullptr;

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg,
                                    WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    // ---- Tray icon notifications ----------------------------
    case WM_TRAYICON:
        if (LOWORD(lParam) == WM_RBUTTONUP ||
            LOWORD(lParam) == WM_CONTEXTMENU)
        {
            Tray_ShowContextMenu(hwnd);
        }
        else if (LOWORD(lParam) == WM_LBUTTONDBLCLK)
        {
            // Double-click opens settings (convenience)
            if (!Settings_IsOpen())
                Settings_Open(g_hInstance);
            else
            {
                // Bring existing settings window to front
                Settings_Open(g_hInstance); // handles bring-to-front internally
            }
        }
        return 0;

    // ---- Tray context menu commands -------------------------
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_TRAY_SETTINGS:
            if (!Settings_IsOpen())
                Settings_Open(g_hInstance);
            else
                Settings_Open(g_hInstance); // brings to front
            return 0;

        case ID_TRAY_EXIT:
            // Signal everything to shut down
            g_running = false;
            PostQuitMessage(0);
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---- Creates the invisible host window ----------------------
static HWND CreateMainWindow(HINSTANCE hInstance)
{
    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = L"OverlayAppMainClass";
    RegisterClassExW(&wc);

    // WS_EX_TOOLWINDOW keeps it off the taskbar
    HWND hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        L"OverlayAppMainClass",
        L"OverlayAppHost",
        WS_OVERLAPPEDWINDOW,
        0, 0, 0, 0,
        nullptr, nullptr, hInstance, nullptr);

    // Keep the window hidden – it exists only to own the message queue
    // and receive tray icon callbacks.
    ShowWindow(hwnd, SW_HIDE);
    return hwnd;
}

// ============================================================
//  WinMain
// ============================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrev*/,
                   LPSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
    g_hInstance = hInstance;

    // ---- Start overlay on its own thread --------------------
    std::thread overlayThread(OverlayThreadFunc);

    // ---- Hidden host window + tray icon ---------------------
    HWND hMain = CreateMainWindow(hInstance);
    if (!hMain)
    {
        g_running = false;
        overlayThread.join();
        return 1;
    }

    if (!Tray_Init(hMain, hInstance))
    {
        g_running = false;
        overlayThread.join();
        return 1;
    }

    // ---- Main message + render loop -------------------------
    // PeekMessage so we can render the settings window each frame
    // without blocking the loop.
    MSG msg = {};
    while (g_running)
    {
        // Process all pending Win32 messages
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                g_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (!g_running) break;

        // Render settings window if it is currently open
        if (Settings_IsOpen())
        {
            Settings_RenderFrame();
        }
        else
        {
            // Nothing to render on the main thread – yield to avoid
            // burning 100 % CPU while the overlay is doing its own vsync.
            Sleep(10);
        }
    }

    // ---- Shutdown -------------------------------------------
    Settings_Close();           // destroy settings window (if open)
    Tray_Destroy();             // remove tray icon

    // Signal overlay thread (if not already set) and wait
    g_running = false;
    overlayThread.join();

    DestroyWindow(hMain);
    UnregisterClassW(L"OverlayAppMainClass", hInstance);

    return 0;
}

#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/// <summary>
/// Creates (or un-hides) the settings window on the CALLING thread.
/// The caller is responsible for running a message + render loop
/// that includes Settings_RenderFrame() each iteration.
/// </summary>
/// <returns>true if the window is now open</returns>
bool Settings_Open(HINSTANCE hInstance);

/// <summary>
/// Hides and destroys the settings window. Safe to call when already closed.
/// </summary>
void Settings_Close();

/// <summary>
/// Returns true while the settings window is alive.
/// </summary>
bool Settings_IsOpen();

/// <summary>
/// Renders one ImGui frame for the settings window.
/// Must be called from the thread that created the window (main thread).
/// </summary>
void Settings_RenderFrame();

#pragma once

/// <summary>
/// Entry point for the overlay thread.
/// Creates a fullscreen, transparent, always-on-top, click-through window,
/// initialises its own D3D11 device + ImGui context, and renders until
/// g_running becomes false.
/// </summary>
void OverlayThreadFunc();

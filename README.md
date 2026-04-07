# OverlayApp – Setup & Build Guide

## Prerequisites
- Visual Studio 2022 (Desktop development with C++ workload)
- Windows SDK 10.0 or later
- Dear ImGui (https://github.com/ocornut/imgui)

---

## Directory Layout

```
OverlayApp/
├── main.cpp
├── overlay.h
├── overlay.cpp
├── settings.h
├── settings.cpp
├── tray.h
├── tray.cpp
├── OverlayApp.vcxproj
└── imgui/                   ← you create this folder
    ├── imgui.h
    ├── imgui.cpp
    ├── imgui_draw.cpp
    ├── imgui_tables.cpp
    ├── imgui_widgets.cpp
    ├── imgui_internal.h
    ├── imconfig.h
    ├── imstb_rectpack.h
    ├── imstb_textedit.h
    ├── imstb_truetype.h
    ├── imgui_impl_win32.h
    ├── imgui_impl_win32.cpp
    ├── imgui_impl_dx11.h
    └── imgui_impl_dx11.cpp
```

---

## Step-by-step

1. **Clone / download Dear ImGui**
   ```
   git clone https://github.com/ocornut/imgui.git tmp_imgui
   ```

2. **Copy required files** into `OverlayApp/imgui/`:
   - From `tmp_imgui/` root:
     `imgui.h`, `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`,
     `imgui_widgets.cpp`, `imgui_internal.h`, `imconfig.h`,
     `imstb_rectpack.h`, `imstb_textedit.h`, `imstb_truetype.h`
   - From `tmp_imgui/backends/`:
     `imgui_impl_win32.h`, `imgui_impl_win32.cpp`,
     `imgui_impl_dx11.h`, `imgui_impl_dx11.cpp`

3. **Open `OverlayApp.vcxproj`** in Visual Studio 2022.

4. **Select `x64`** platform (the project only targets x64).

5. **Build** → `Debug` or `Release`.

---

## Architecture Summary

| Component         | Thread      | D3D11 Device | ImGui Context | Role                          |
|-------------------|-------------|--------------|---------------|-------------------------------|
| Overlay Window    | Own thread  | Own          | Own           | Transparent full-screen HUD   |
| Settings Window   | Main thread | Own          | Own           | ImGui UI panel                |
| Hidden Main Wnd   | Main thread | —            | —             | Message loop + tray icon      |

### Key design decisions
- `g_running` (`std::atomic<bool>`) is the single shutdown signal.
- Overlay thread checks `g_running` every frame; exits its loop when `false`.
- The main thread uses `PeekMessage` so it can call `Settings_RenderFrame()`
  each iteration without stalling.
- Closing the Settings window calls `Settings_Close()` which destroys only
  the settings DX11 device + ImGui context; the overlay is unaffected.
- Tray → Exit posts `WM_QUIT` and sets `g_running = false`, after which
  `main()` joins the overlay thread before exiting.

### Overlay transparency
The overlay window uses:
- `WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT` (click-through, always on top)
- `SetLayeredWindowAttributes(hwnd, RGB(0,0,0), 0, LWA_COLORKEY)`
- DX11 clears the back buffer to pure black `{0,0,0,1}`

Any pixel left as pure black is rendered transparent.  ImGui draws only
with non-black colors, so all HUD text/shapes are visible.

---

## Extending

- To add more overlay visuals: edit the `ImGui::Begin("##hud", …)` block
  in `overlay.cpp`.
- To add more settings controls: edit `Settings_RenderFrame()` in
  `settings.cpp`.
- To persist settings: write/read a simple INI/JSON at startup and
  `Settings_Close()`.

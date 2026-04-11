# g_ScopeOverlay

> ⚠️ **Early Beta** — This project is actively being developed. Expect breaking changes, missing features, and rough edges as more functionality is added over time. If you run into any bugs or issues, please [open an issue](../../issues) — it helps a lot!

A lightweight Windows screen magnifier overlay built for **PVP games** — it simulates a scope by capturing and zooming in on a region of your screen in real time. Also useful as a general accessibility magnifier.

---

## Features

- Real-time screen region capture with GPU acceleration (no CPU readback)
- Works alongside any game or application — no integrations, hooks, injections, or mods required
- Adjustable capture position, size, and zoom level (1×–8×)
- Optional rounded corners on the zoomed view
- Settings panel that appears when the overlay window is focused
- FPS display (optional)
- VSync toggle

---

## Requirements

- Windows 10 or Windows 11 (64-bit)
- A GPU that supports DirectX 11
- Visual Studio 2022 with the **Desktop development with C++** workload
- Windows SDK 10.0 or later
- [Dear ImGui](https://github.com/ocornut/imgui)

---

## Usage

Run `ScopeOverlay.exe`. The overlay appears immediately as a transparent full-screen window.

**To open the settings panel**, click the overlay window in the taskbar (or Alt+Tab to it). The settings panel will appear.

| Setting | Description |
|---|---|
| **x / y** | Center position of the captured region (screen pixels) |
| **dx / dy** | Width and height of the captured region |
| **zoom** | How much to magnify the captured region (1×–8×) |
| **roundness** | Corner radius of the displayed zoomed image |
| **FPS Overlay** | Show a color-coded frame rate counter on screen |
| **VSYNC** | Limit rendering to your monitor's refresh rate |
| **Exit** | Close the program |

**To hide the settings panel**, click anywhere outside it or switch to another application — the overlay becomes click-through again automatically.

---

## Known Issues

- **After waking from sleep:** The zoomed view may freeze on the last captured frame. Workaround: restart the application.

---

## Disclaimer

g_ScopeOverlay is not a cheat — it uses the same principle as the Windows built-in Magnifier. However, it may be considered unfair in competitive play and could conflict with anti-cheat systems. **Use it at your own risk.**

---

## Building from Source

### 1. Get Dear ImGui

```
git clone https://github.com/ocornut/imgui.git tmp_imgui
```

### 2. Copy the required ImGui files into `imgui/`

From the `tmp_imgui/` root, copy:

```
imgui.h  imgui.cpp  imgui_draw.cpp  imgui_tables.cpp  imgui_widgets.cpp
imgui_internal.h  imconfig.h
imstb_rectpack.h  imstb_textedit.h  imstb_truetype.h
```

From `tmp_imgui/backends/`, copy:

```
imgui_impl_win32.h  imgui_impl_win32.cpp
imgui_impl_dx11.h   imgui_impl_dx11.cpp
```

Your folder should look like this:

```
g_ScopeOverlay/
├── main.cpp
├── g_ScopeOverlay.vcxproj
└── imgui/
    ├── imgui.h
    ├── imgui.cpp
    └── ... (all files listed above)
```

### 3. Build

1. Open `g_ScopeOverlay.vcxproj` in Visual Studio 2022.
2. Select the **x64** platform.
3. Build in **Debug**.

---

## Prior Art

This project is functionally similar to [Scope X](https://centerpointgaming.com/scopex.html), a commercial screen magnifier overlay. g_ScopeOverlay was developed independently — the similarity was only discovered after the first versions were already built. g_ScopeOverlay is fully open-source under the Apache License 2.0.

---

## License

This project is licensed under the **Apache License 2.0**. See [LICENSE](LICENSE) for details.

This project uses [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut, licensed under the MIT License.

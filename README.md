# g_ScopeOverlay

> ⚠️ **Early Beta**
> This project is actively being developed. Expect breaking changes, missing features, and rough edges as more functionality is added over time.
> If you run into a bug or issue, please [open an issue](../../issues).

A lightweight Windows screen magnifier overlay that captures and zooms in on a selected region of your screen in real time.

It can be used as a general accessibility magnifier, and may also be useful for games or applications where a temporary zoomed-in view is helpful.

---

## Star Goal

If g_ScopeOverlay reaches **2 GitHub stars**, I will start working on a more polished **non-beta release**.

If you find the project useful, consider giving it a star. It helps show that there is interest in continued development.

🙏 ⭐ 👉👈
---

## Visual Showcase

<p align="center">
  <img src="README-assets/g_ScopeOverlay.gif" alt="g_ScopeOverlay demo" width="700">
</p>

---

## Download

No coding or build setup is required if you only want to use the app.

Download the latest `.exe` from the [Releases](../../releases) page.

---

## Quick Start

1. Download the latest release.
2. Run `ScopeOverlay.exe`.
3. The overlay appears as a transparent full-screen window.
4. Click the overlay in the taskbar, or Alt+Tab to it, to open the settings panel.
5. Adjust the capture position, size, zoom, and display options.

To hide the settings panel, click anywhere outside it or switch to another application. The overlay becomes click-through again automatically.

---

## Features
* **Real-time screen magnification**<br>Captures and zooms a region of your screen in real time.
* **No game integration required**<br>No hooks, injections, memory reading, mods, or game file changes.
* **Adjustable capture area**<br>Change the capture position, size, and zoom level.
* **Optional rounded corners**<br>Customize the shape of the zoomed view.
* **Optional FPS display**<br>Show a color-coded frame rate counter on screen.
* **VSync toggle**<br>Limit rendering to your monitor’s refresh rate.

---

## Settings

| Setting         | Description                                              |
| --------------- | -------------------------------------------------------- |
| **x / y**       | Center position of the captured region, in screen pixels |
| **dx / dy**     | Width and height of the captured region                  |
| **zoom**        | Magnification level, from 1× to 8×                       |
| **roundness**   | Corner radius of the displayed zoomed image              |
| **FPS Overlay** | Shows or hides the frame rate counter                    |
| **VSYNC**       | Limits rendering to your monitor’s refresh rate          |
| **Exit**        | Closes the program                                       |

---

## Requirements for Running the App

These are the only requirements if you download the `.exe` from the Releases page.

* Windows 10 or Windows 11, 64-bit
* A GPU that supports DirectX 11

You do **not** need Visual Studio, the Windows SDK, or Dear ImGui if you are only downloading and running the release build.

---

## Known Issues

* **Not recordable:** The overlay may not appear in screen recordings, screenshots, or streaming software.
* Some games or anti-cheat systems may block, hide, flag, or interfere with third-party overlays.

---

## Fair Use, Anti-Cheat, and Legal Disclaimer

g_ScopeOverlay is a screen magnification overlay. It does not modify game files, read game memory, inject code, hook into game processes, automate input, or bypass security systems.

However, some games and anti-cheat systems may still classify third-party overlays, screen magnifiers, or visual assistance tools as disallowed software. Rules vary by game, server, league, tournament, and platform.

Using this tool in online, ranked, competitive, or tournament play may violate a game’s Terms of Service, Code of Conduct, tournament rules, or anti-cheat policy, even if the tool does not directly interact with the game.

Use g_ScopeOverlay at your own risk. You are responsible for checking and following the rules of any game, service, platform, or competition where you use it. The project maintainers are not responsible for account warnings, restrictions, bans, competitive penalties, or other consequences resulting from use of this software.

This project is provided for accessibility, experimentation, and general screen magnification purposes. It is not intended to bypass anti-cheat systems or provide an unfair advantage in competitive environments.

---

## Building from Source

These steps are only needed if you want to modify the code or build the app yourself.

### Build Requirements

* Visual Studio 2022
* **Desktop development with C++** workload
* Windows SDK 10.0 or later
* [Dear ImGui](https://github.com/ocornut/imgui)

---

### 1. Get Dear ImGui

```bash
git clone https://github.com/ocornut/imgui.git tmp_imgui
```

---

### 2. Copy the Required ImGui Files

From the `tmp_imgui/` root folder, copy these files into the project’s `imgui/` folder:

```text
imgui.h
imgui.cpp
imgui_draw.cpp
imgui_tables.cpp
imgui_widgets.cpp
imgui_internal.h
imconfig.h
imstb_rectpack.h
imstb_textedit.h
imstb_truetype.h
```

From `tmp_imgui/backends/`, copy:

```text
imgui_impl_win32.h
imgui_impl_win32.cpp
imgui_impl_dx11.h
imgui_impl_dx11.cpp
```

Your folder should look like this:

```text
g_ScopeOverlay/
├── main.cpp
├── g_ScopeOverlay.vcxproj
└── imgui/
    ├── imgui.h
    ├── imgui.cpp
    └── ... other ImGui files
```

---

### 3. Build the Project

1. Open `g_ScopeOverlay.vcxproj` in Visual Studio 2022.
2. Select the **x64** platform.
3. Build in **Debug** or **Release**.

---

## Project Status

g_ScopeOverlay is currently in early beta. Features, behavior, and project structure may change between releases.

Planned improvements may include:

* Better configuration saving
* More overlay shape options
* Improved multi-monitor behavior
* Better recording / capture compatibility, if technically possible
* Installer or portable release improvements

---

## License

This project is licensed under the **Apache License 2.0**. See [LICENSE](LICENSE) for details.

This project uses [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut, licensed under the MIT License.

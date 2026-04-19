#pragma once
namespace Config
{
	// ── Output window ────────────────────────────────────────────────────
	inline int    OutW = 400;       // width  of output
	inline int    OutH = 400;       // height of output

	// ── Position ─────────────────────────────────────────────────────────
	inline int    PosX = 200;     // center X of capture and output
	inline int    PosY = 200;     // center Y of capture and output

	// ── Magnification ────────────────────────────────────────────────────
	inline float  Zoom = 2.0f;    // zoom level
	inline bool  ToggleToZoom = false;		// false = show only while key held
											// true = key toggles on/off

	// ── Visuals ──────────────────────────────────────────────────────────
	inline float  Rounding = 0.0f;   // ImGui AddImageRounded corner radius
	inline bool   Vsync = true;   // DXGI present interval 1 vs 0

	// ── Overlay ──────────────────────────────────────────────────────────
	inline bool   ShowFpsOverlay = false; // shows the zoom refresh rate
	inline bool   ShowZoomOverlay = false; // shows the zoom refresh rate

	// ── Keybind ──────────────────────────────────────────────────────────
	struct gKeybind
	{
		int key;
		bool change = false;
		bool pressed = false;
	};

	inline gKeybind ZoomKeybind = { 0xBB, false, false }; // default: VK_OEM_PLUS

	inline int CaptW() { return OutW / Zoom; }
	inline int CaptH() { return OutH / Zoom; }
}
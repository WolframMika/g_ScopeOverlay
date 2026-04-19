#pragma once

#include "gBackend.h"
#include "gCapture.h"
#include "Config.h"

namespace Render {
	extern ImGuiIO* io;
	extern ImGuiStyle* style;
	extern gCapture* screen_capture;
	extern HWND hwnd;
	extern int screenW;
	extern int screenH;
	extern unsigned int g_mode;

	void render();
	static void HelpMarker(const char* desc);
	static void FpsMarker(float Framerate);
	const char* GetKeyName(int vk);
}
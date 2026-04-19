#include "gBackend.h"
#include "gCapture.h"
#include "Config.h"


/*
* Known Issues:
* - **Not recordable:** The overlay cannot be captured by screen recording or streaming software.
* Notes:
* 26 egzaminas lol
* no vsync option (GUI with hardcoded vsync and Capture has his own weird shit)
* no multi-thread (multi-threading is not that effective in this case. 
	Issues with device contexts make it not worth it,
	and this approach is already very fast since most of the work is handled by the GPU)
* is it going to capture screen while not showing image? No.
* To do:
* - better file/code framework
* - multi-screen
* - true fps (a true fps of capture)
*/



bool g_done = false;

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Main code
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// Make process DPI aware and obtain main monitor scale
	ImGui_ImplWin32_EnableDpiAwareness();
	float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

	// Create application window
	WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
	::RegisterClassExW(&wc);

	screenW = ::GetSystemMetrics(SM_CXSCREEN);
	screenH = ::GetSystemMetrics(SM_CYSCREEN);

	Config::PosX = (screenW) / 2;
	Config::PosY = (screenH) / 2;

	hwnd = ::CreateWindowExW(
		WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
		wc.lpszClassName, L"Overlay", WS_POPUP,
		0, 0, screenW, screenH,
		nullptr, nullptr, wc.hInstance, nullptr);

	MARGINS margins = { -1, -1, -1, -1 };
	DwmExtendFrameIntoClientArea(hwnd, &margins);
	SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
	::SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);

	// Initialize Direct3D
	if (!gBackend::CreateDeviceD3D(hwnd))
	{
		gBackend::CleanupDeviceD3D();
		::UnregisterClassW(wc.lpszClassName, wc.hInstance);
		return 1;
	}

	// Create capture
	gSizePos tSizePos = { Config::CaptW(), Config::CaptH(), Config::PosX, Config::PosY };
	screen_capture = new gCapture(tSizePos, gBackend::pd3dDevice, gBackend::pd3dDeviceContext);

	// Show the window
	::ShowWindow(hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(hwnd);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	io = &ImGui::GetIO(); (void)io;
	io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	// Setup scaling
	style = &ImGui::GetStyle();
	style->ScaleAllSizes(main_scale);
	style->FontScaleDpi = main_scale;
	//io.ConfigDpiScaleFonts = true;

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(gBackend::pd3dDevice, gBackend::pd3dDeviceContext);

	// Load Fonts
	io->Fonts->AddFontDefaultVector();

	while (!g_done)
	{
		// Poll and handle messages (inputs, window resize, etc.)
		// See the WndProc() function below for our to dispatch events to the Win32 backend.
		MSG msg;
		while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				g_done = true;
		}
		if (g_done)
			break;

		// Handle window being minimized or screen locked
		if (gBackend::SwapChainOccluded && gBackend::pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
		{
			::Sleep(10);
			continue;
		}
		gBackend::SwapChainOccluded = false;

		// Handle window resize (we don't resize directly in the WM_SIZE handler)
		if (gBackend::ResizeWidth != 0 && gBackend::ResizeHeight != 0)
		{
			gBackend::CleanupRenderTarget();
			gBackend::pSwapChain->ResizeBuffers(0, gBackend::ResizeWidth, gBackend::ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
			gBackend::ResizeWidth = gBackend::ResizeHeight = 0;
			gBackend::CreateRenderTarget();
		}

		//Handle zoom keybind
		if (g_zoom_keybind.key != 0 || !g_zoom_keybind.change)
		{
			SHORT state = GetAsyncKeyState(g_zoom_keybind.key);

			bool isDown = (state & 0x8000); // key currently held

			if (Config::ToggleToZoom)
			{
				// toggle on press edge
				if (isDown && !g_zoom_keybind.pressed)
				{
					g_zoom_keybind.pressed = true;
					Config::ShowZoomOverlay = !Config::ShowZoomOverlay;
				}
				else if (!isDown)
				{
					g_zoom_keybind.pressed = false;
				}
			}
			else
			{
				Config::ShowZoomOverlay = isDown;
			}
		}

		render();
	}

	// Cleanup
	if (screen_capture) { delete screen_capture; screen_capture = nullptr; }
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	gBackend::CleanupDeviceD3D();
	::DestroyWindow(hwnd);
	::UnregisterClassW(wc.lpszClassName, wc.hInstance);

	return 0;
}



// Helper functions
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return 1;

	switch (msg)
	{
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
			return 0;
		gBackend::ResizeWidth = (UINT)LOWORD(lParam);
		gBackend::ResizeHeight = (UINT)HIWORD(lParam);
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU)
			return 0;
		break;
	case WM_KEYDOWN:
	{
		int key = (int)wParam;

		if (g_zoom_keybind.change && key == VK_ESCAPE)
		{
			g_zoom_keybind.change = false;
			g_zoom_keybind.key = 0;
			return 0;
		}

		if (g_zoom_keybind.change)
		{
			g_zoom_keybind.key = key;
			g_zoom_keybind.change = false;
			return 0;
		}

		break;
	}
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_XBUTTONDOWN:
	{
		if (g_zoom_keybind.change)
		{
			if (msg == WM_LBUTTONDOWN) g_zoom_keybind.key = VK_LBUTTON;
			if (msg == WM_RBUTTONDOWN) g_zoom_keybind.key = VK_RBUTTON;
			if (msg == WM_MBUTTONDOWN) g_zoom_keybind.key = VK_MBUTTON;
			if (msg == WM_XBUTTONDOWN)
			{
				int btn = GET_XBUTTON_WPARAM(wParam);
				g_zoom_keybind.key = (btn == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;
			}

			g_zoom_keybind.change = false;
		}
		break;
	}
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}



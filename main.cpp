#include "gCapture.h"
#include "gBackend.h"

/*
* Known Issues:
* - **Not recordable:** The overlay cannot be captured by screen recording or streaming software.
* - Doesn't work without imgui_demo.cpp
* To do:
* - better gCapture
* - clean up code
* - make it more readable
* - multi-thread
* - multi-file
* - multi-screen
*/

ImGuiIO* io = nullptr;
ImGuiStyle* style = nullptr;
gCapture* screen_capture = nullptr;
HWND hwnd;
int screenW;
int screenH;

// Screen capture state
gSizePos g_SP = { 400, 400, 0, 0 };
static float g_zoom = 2.0f;
static float g_round = 0.0f;

bool g_show_fps_overlay = false;
bool g_done = false;
unsigned int g_mode = 0;
bool g_vsync = true;

bool g_toggle_zoom = false;
bool g_show_zoom = false;

struct gKeybind
{
	int key = 0;
	bool change = false;
	bool pressed = false;
};

gKeybind g_zoom_keybind = { VK_OEM_PLUS, false, false };


void render();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void HelpMarker(const char* desc);
static void FPSMarker(float scale = 0, ImVec2 offset = ImVec2(0.0, 0.0));
const char* GetKeyName(int vk);

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

	g_SP.x = (screenW) / 2;
	g_SP.y = (screenH) / 2;

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
	screen_capture = new gCapture(g_SP, gBackend::pd3dDevice, gBackend::pd3dDeviceContext);

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

			if (g_toggle_zoom)
			{
				// toggle on press edge
				if (isDown && !g_zoom_keybind.pressed)
				{
					g_zoom_keybind.pressed = true;
					g_show_zoom = !g_show_zoom;
				}
				else if (!isDown)
				{
					g_zoom_keybind.pressed = false;
				}
			}
			else
			{
				g_show_zoom = isDown;
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

void render() {
	// Start the Dear ImGui frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Fullscreen transparent non-interactive background
	if (g_show_zoom) {
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(io->DisplaySize);
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::Begin("##overlay_bg", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);

		if (g_show_fps_overlay)
			FPSMarker(2.0, ImVec2(8, 8));

		// ??? Bug found: After PC wakes up from sleep screen region doesn't update and it is stuck at same frame
		screen_capture->capture({ (unsigned int)(g_SP.dx / g_zoom), (unsigned int)(g_SP.dy / g_zoom), g_SP.x, g_SP.y });

		ID3D11ShaderResourceView* captureTexSRV = screen_capture->get();

		if (captureTexSRV)
		{
			ImVec2 pos = ImVec2((float)g_SP.x - (g_SP.dx / 2), (float)g_SP.y - (g_SP.dy / 2));
			ImVec2 size = ImVec2(g_SP.dx, g_SP.dy);

			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			draw_list->AddImageRounded(
				(ImTextureID)captureTexSRV,
				pos,
				ImVec2(pos.x + size.x, pos.y + size.y),
				ImVec2(0, 0),
				ImVec2(1, 1),
				IM_COL32_WHITE,
				g_round
			);
		}

		ImGui::End();
	}

	// Show settings window when our window is focused.
	if (::GetForegroundWindow() == hwnd)
	{
		ImGui::Begin("g_ScopeOverlay Setting", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar);

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::MenuItem("Exit", "Alt+F4"))
			{
				g_done = true;
			}

			if (ImGui::MenuItem("Region"))
			{
				g_mode = 0;
			}

			if (ImGui::MenuItem("FPS"))
			{
				g_mode = 1;
			}

			if (ImGui::MenuItem("Keybind"))
			{
				g_mode = 2;
			}

			ImGui::EndMenuBar();
		}

		ImGui::PushFont(NULL, style->FontSizeBase * 1.2);

		switch (g_mode)
		{
		case 1:
			ImGui::Text("FPS settings");
			FPSMarker();
			ImGui::Checkbox("FPS Overlay", &g_show_fps_overlay);
			ImGui::Checkbox("VSYNC", &g_vsync);

			break;

		case 2:
			ImGui::Text("Keybind settings");

			if (ImGui::Button("Set Keybind"))
			{
				g_zoom_keybind.change = true;
			}

			ImGui::Text("Key: %s", (g_zoom_keybind.change ? "Press a key..." : GetKeyName(g_zoom_keybind.key)));

			ImGui::Checkbox("Is Toggle Zoom", &g_toggle_zoom);

			break;

		default:

			unsigned int step = 1;
			unsigned int step_fast = 10;
			float max_round = min((g_SP.dx) / 2, (g_SP.dy) / 2);

			ImGui::Text("Capture Region");

			ImGui::SeparatorText("Position");

			if (ImGui::InputScalar("x", ImGuiDataType_U32, &g_SP.x, &step, &step_fast)) {
				g_SP.x = min(screenW - g_SP.dx / 2, max(0 + g_SP.dx / 2, g_SP.x));
			}

			if (ImGui::InputScalar("y", ImGuiDataType_U32, &g_SP.y, &step, &step_fast)) {
				g_SP.y = min(screenH - g_SP.dy / 2, max(0 + g_SP.dy / 2, g_SP.y));
			}

			if (ImGui::Button("Center to screen")) {
				g_SP.x = (screenW) / 2;
				g_SP.y = (screenH) / 2;
			}

			ImGui::SameLine(); HelpMarker("Position is center of capture region. \n Hold Ctrl to adjust faster.");

			ImGui::SeparatorText("Size");

			if (ImGui::InputScalar("dx", ImGuiDataType_U32, &g_SP.dx, &step, &step_fast)) {
				g_SP.dx = min(screenW, g_SP.dx);
				g_SP.x = min(screenW - g_SP.dx / 2, max(0 + g_SP.dx / 2, g_SP.x));
				g_round = min(max_round, g_round);
			}

			if (ImGui::InputScalar("dy", ImGuiDataType_U32, &g_SP.dy, &step, &step_fast)) {
				g_SP.dy = min(screenH, g_SP.dy);
				g_SP.y = min(screenH - g_SP.dy / 2, max(0 + g_SP.dy / 2, g_SP.y));
				g_round = min(max_round, g_round);
			}

			ImGui::SeparatorText("Appearance");

			ImGui::SliderFloat("zoom", &g_zoom, 1.0f, 8.0f, "%.1fx");

			if (ImGui::SliderFloat("roundness", &g_round, 0.0f, max_round, "%.1fx")) {
				g_round = min(max_round, g_round);
			}

			break;
		}

		ImGui::PopFont();
		ImGui::End();
	}

	// Toggle click-through based on mouse capture
	LONG_PTR exStyle = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
	if (io->WantCaptureMouse)
		exStyle &= ~WS_EX_TRANSPARENT;
	else
		exStyle |= WS_EX_TRANSPARENT;
	::SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);

	// Rendering
	ImGui::Render();
	const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	gBackend::pd3dDeviceContext->OMSetRenderTargets(1, &gBackend::mainRenderTargetView, nullptr);
	gBackend::pd3dDeviceContext->ClearRenderTargetView(gBackend::mainRenderTargetView, clear_color);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	HRESULT hr = gBackend::pSwapChain->Present(g_vsync, 0); //vsync
	gBackend::SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
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


// Helper to display a little (?) mark which shows a tooltip when hovered.
// In your own code you may want to display an actual icon if you are using a merged icon fonts (see docs/FONTS.md)
static void HelpMarker(const char* desc)
{
	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip())
	{
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

// Helper to display a FPS multy-colored mark.
static void FPSMarker(float scale, ImVec2 offset)
{
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGuiStyle& style = ImGui::GetStyle();

	ImVec2 pos = ImGui::GetCursorPos();
	ImGui::SetCursorPos(ImVec2(pos.x + offset.x, pos.y + offset.y));
	ImGui::PushFont(NULL, style.FontSizeBase * scale);
	if (io.Framerate < 15)
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%.1f FPS", io.Framerate);
	else if (io.Framerate < 60)
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%.1f FPS", io.Framerate);
	else
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%.1f FPS", io.Framerate);
	ImGui::PopFont();
}

// Helper to get key name from virtual key code
const char* GetKeyName(int vk)
{
	if (vk == 0) return "None";

	static char name[64];

	switch (vk)
	{
	case VK_LBUTTON:   return "Left Mouse";
	case VK_RBUTTON:   return "Right Mouse";
	case VK_MBUTTON:   return "Middle Mouse";
	case VK_XBUTTON1:  return "Mouse X1";
	case VK_XBUTTON2:  return "Mouse X2";
	}

	UINT scanCode = MapVirtualKey(vk, MAPVK_VK_TO_VSC);

	if (vk == VK_SHIFT)   scanCode = MapVirtualKey(VK_LSHIFT, MAPVK_VK_TO_VSC);
	if (vk == VK_CONTROL) scanCode = MapVirtualKey(VK_LCONTROL, MAPVK_VK_TO_VSC);
	if (vk == VK_MENU)    scanCode = MapVirtualKey(VK_LMENU, MAPVK_VK_TO_VSC);

	GetKeyNameTextA(scanCode << 16, name, sizeof(name));
	return name;
}
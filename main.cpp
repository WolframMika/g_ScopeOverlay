#include "gCapture.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <algorithm>
#include <dwmapi.h>
#include <windows.h>
#pragma comment(lib, "dwmapi.lib")
#include <dxgi1_2.h>
#pragma comment(lib, "dxgi.lib")

#define IMGUI_DEFINE_MATH_OPERATORS



/*
* Known Issues:
* - **After waking from sleep:** The zoomed view may freeze on the last captured frame. Workaround: restart the application.
* - **Not recordable:** The overlay cannot be captured by screen recording or streaming software.
* - Deosn't work without imgui_demo.cpp
*/

// Data
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static bool                     g_SwapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// Screen capture state
gSizePos g_SP = { 400, 400, 0, 0 };
static float g_zoom = 2.0f;
static float g_round = 0.0f;

bool g_show_fps_overlay = false;
bool g_done = false;
unsigned int g_mode = 0;
bool g_vsync = true;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);

void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void HelpMarker(const char* desc);
static void FPSMarker(float scale = 0, ImVec2 offset = ImVec2(0.0, 0.0));

// Main code
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// Make process DPI aware and obtain main monitor scale
	ImGui_ImplWin32_EnableDpiAwareness();
	float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

	// Create application window
	WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
	::RegisterClassExW(&wc);

	int screenW = ::GetSystemMetrics(SM_CXSCREEN);
	int screenH = ::GetSystemMetrics(SM_CYSCREEN);

	g_SP.x = (screenW) / 2;
	g_SP.y = (screenH) / 2;

	HWND hwnd = ::CreateWindowExW(
		WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
		wc.lpszClassName, L"Overlay", WS_POPUP,
		0, 0, screenW, screenH,
		nullptr, nullptr, wc.hInstance, nullptr);

	MARGINS margins = { -1, -1, -1, -1 };
	DwmExtendFrameIntoClientArea(hwnd, &margins);
	SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
	::SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);

	// Initialize Direct3D
	if (!CreateDeviceD3D(hwnd))
	{
		CleanupDeviceD3D();
		::UnregisterClassW(wc.lpszClassName, wc.hInstance);
		return 1;
	}

	// Create capture
	gCapture* screen_capture = new gCapture(g_SP, g_pd3dDevice, g_pd3dDeviceContext);

	// Show the window
	::ShowWindow(hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(hwnd);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	// Setup scaling
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(main_scale);
	style.FontScaleDpi = main_scale;
	//io.ConfigDpiScaleFonts = true;

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

	// Load Fonts
	io.Fonts->AddFontDefaultVector();

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
		if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
		{
			::Sleep(10);
			continue;
		}
		g_SwapChainOccluded = false;

		// Handle window resize (we don't resize directly in the WM_SIZE handler)
		if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
		{
			CleanupRenderTarget();
			g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
			g_ResizeWidth = g_ResizeHeight = 0;
			CreateRenderTarget();
		}

		// Start the Dear ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// Fullscreen transparent non-interactive background
		{
			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::SetNextWindowSize(io.DisplaySize);
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
			ImGui::Begin("g_ScopeOverlay Setting", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar); //finish menu

			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("Exit"))
					{
						g_done = true;
					}
					ImGui::EndMenu();
				}

				ImGui::EndMenuBar();
			}

			ImGui::PushFont(NULL, style.FontSizeBase * 1.2);

			FPSMarker();
			ImGui::Checkbox("FPS Overlay", &g_show_fps_overlay);
			ImGui::Checkbox("VSYNC", &g_vsync);

			ImGui::Separator();

			unsigned int step = 1;
			unsigned int step_fast = 10;
			float max_round = min((g_SP.dx) / 2, (g_SP.dy) / 2);

			ImGui::Text("Capture Region");

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

			ImGui::SliderFloat("zoom", &g_zoom, 1.0f, 8.0f, "%.1fx");

			if (ImGui::SliderFloat("roundness", &g_round, 0.0f, max_round, "%.1fx")) {
				g_round = min(max_round, g_round);
			}

			ImGui::Separator();

			if (ImGui::Button("Exit g_ScopeOverlay"))
				g_done = true;
			ImGui::SameLine(); HelpMarker("Fully closes the program.");

			ImGui::PopFont();
			ImGui::End();
		}

		// Toggle click-through based on mouse capture
		LONG_PTR exStyle = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
		if (io.WantCaptureMouse)
			exStyle &= ~WS_EX_TRANSPARENT;
		else
			exStyle |= WS_EX_TRANSPARENT;
		::SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);

		// Rendering
		ImGui::Render();
		const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
		g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		HRESULT hr = g_pSwapChain->Present(g_vsync, 0); //vsync
		g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
	}

	// Cleanup
	if (screen_capture) { delete screen_capture; screen_capture = nullptr; }
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	CleanupDeviceD3D();
	::DestroyWindow(hwnd);
	::UnregisterClassW(wc.lpszClassName, wc.hInstance);

	return 0;
}

// Helper functions
bool CreateDeviceD3D(HWND hWnd)
{
	// Setup swap chain
	// This is a basic setup.
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT createDeviceFlags = 0;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
	if (res == DXGI_ERROR_UNSUPPORTED)
		res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
	if (res != S_OK)
		return false;

	CreateRenderTarget();
	return true;
}

void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer;
	g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
	pBackBuffer->Release();
}

void CleanupRenderTarget()
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
			return 0;
		g_ResizeWidth = (UINT)LOWORD(lParam);
		g_ResizeHeight = (UINT)HIWORD(lParam);
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU)
			return 0;
		break;
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

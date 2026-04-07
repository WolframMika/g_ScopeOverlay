// ============================================================
//  overlay.cpp
//  Fullscreen transparent overlay window.
//  Runs entirely on its own thread.
//  Owns its own D3D11 device, swap chain and ImGui context.
// ============================================================
#include "overlay.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <atomic>

// ImGui headers (place the imgui source tree next to these files)
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// ---- globals shared with main.cpp ----------------------------
extern std::atomic<bool> g_running;

// Forward-declare ImGui Win32 message handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ---- local D3D11 state ---------------------------------------
namespace
{
    struct OverlayDX
    {
        ID3D11Device*           device          = nullptr;
        ID3D11DeviceContext*    context         = nullptr;
        IDXGISwapChain*         swapChain       = nullptr;
        ID3D11RenderTargetView* renderTargetView = nullptr;
    };

    // ---- D3D11 helpers ---------------------------------------
    static bool DX_Init(HWND hwnd, OverlayDX& dx)
    {
        DXGI_SWAP_CHAIN_DESC sd              = {};
        sd.BufferCount                       = 2;
        sd.BufferDesc.Format                 = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator  = 60;
        sd.BufferDesc.RefreshRate.Denominator= 1;
        sd.Flags                             = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage                       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow                      = hwnd;
        sd.SampleDesc.Count                  = 1;
        sd.Windowed                          = TRUE;
        sd.SwapEffect                        = DXGI_SWAP_EFFECT_DISCARD;

        const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0,
                                             D3D_FEATURE_LEVEL_10_0 };
        D3D_FEATURE_LEVEL featureLevel    = {};

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            0, levels, 2, D3D11_SDK_VERSION,
            &sd, &dx.swapChain, &dx.device, &featureLevel, &dx.context);

        if (FAILED(hr)) return false;

        ID3D11Texture2D* pBackBuffer = nullptr;
        hr = dx.swapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        if (FAILED(hr)) return false;

        hr = dx.device->CreateRenderTargetView(pBackBuffer, nullptr,
                                               &dx.renderTargetView);
        pBackBuffer->Release();
        return SUCCEEDED(hr);
    }

    static void DX_Cleanup(OverlayDX& dx)
    {
        if (dx.renderTargetView) { dx.renderTargetView->Release(); dx.renderTargetView = nullptr; }
        if (dx.swapChain)        { dx.swapChain->Release();        dx.swapChain        = nullptr; }
        if (dx.context)          { dx.context->Release();          dx.context          = nullptr; }
        if (dx.device)           { dx.device->Release();           dx.device           = nullptr; }
    }

    // ---- Window procedure ------------------------------------
    static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg,
                                           WPARAM wParam, LPARAM lParam)
    {
        // Route to ImGui (won't do much for a transparent/click-through window,
        // but keeps the plumbing correct)
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
            return TRUE;

        switch (msg)
        {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
} // anonymous namespace

// ==============================================================
void OverlayThreadFunc()
{
    HINSTANCE hInst = GetModuleHandleW(nullptr);

    // ---- Register window class --------------------------------
    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = OverlayWndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"OverlayWndClass";
    RegisterClassExW(&wc);

    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);

    // ---- Create transparent, click-through, always-on-top window
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        L"OverlayWndClass",
        L"Overlay",
        WS_POPUP,
        0, 0, screenW, screenH,
        nullptr, nullptr, hInst, nullptr);

    if (!hwnd)
    {
        g_running = false;
        return;
    }

    // Color-key: pure black (0x000000) will be rendered transparent.
    // DirectX clears the back buffer to this color each frame.
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd);

    // ---- Init D3D11 ------------------------------------------
    OverlayDX dx;
    if (!DX_Init(hwnd, dx))
    {
        DestroyWindow(hwnd);
        UnregisterClassW(L"OverlayWndClass", hInst);
        g_running = false;
        return;
    }

    // ---- Init ImGui (this thread owns the context) -----------
    ImGuiContext* imguiCtx = ImGui::CreateContext();
    ImGui::SetCurrentContext(imguiCtx);

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;                           // no .ini persistence
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(dx.device, dx.context);

    // ---- Render loop -----------------------------------------
    MSG msg = {};
    while (g_running)
    {
        // Drain window messages
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
            {
                g_running = false;
                break;
            }
        }

        if (!g_running) break;

        // ---- Begin ImGui frame -------------------------------
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // ---- Overlay content ---------------------------------
        // Example: a small HUD-style info panel in the top-left corner.
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(220.0f, 80.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);  // fully transparent background

        ImGuiWindowFlags overlayFlags =
            ImGuiWindowFlags_NoTitleBar        |
            ImGuiWindowFlags_NoResize          |
            ImGuiWindowFlags_NoScrollbar       |
            ImGuiWindowFlags_NoInputs          |
            ImGuiWindowFlags_NoNav             |
            ImGuiWindowFlags_NoMove            |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoDecoration;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("##hud", nullptr, overlayFlags);

        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
                           "Overlay Running");
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
                           "FPS: %.1f", io.Framerate);
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f),
                           "Right-click tray for menu");

        ImGui::End();
        ImGui::PopStyleVar();

        // ---- Render ------------------------------------------
        ImGui::Render();

        // Clear to pure black  →  color-key makes it transparent
        const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        dx.context->OMSetRenderTargets(1, &dx.renderTargetView, nullptr);
        dx.context->ClearRenderTargetView(dx.renderTargetView, clearColor);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        dx.swapChain->Present(1, 0);    // vsync
    }

    // ---- Cleanup ---------------------------------------------
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(imguiCtx);

    DX_Cleanup(dx);

    DestroyWindow(hwnd);
    UnregisterClassW(L"OverlayWndClass", hInst);
}

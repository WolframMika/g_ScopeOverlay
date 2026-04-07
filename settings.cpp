// ============================================================
//  settings.cpp
//  Standard settings window: title bar, D3D11, ImGui.
//  Runs on the main thread alongside the tray message loop.
//  Closing the window hides it but does NOT kill the overlay.
// ============================================================
#include "settings.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// Forward-declare ImGui Win32 handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ============================================================
//  Internal state
// ============================================================
namespace
{
    // ---- D3D11 resources ------------------------------------
    struct SettingsDX
    {
        ID3D11Device*           device           = nullptr;
        ID3D11DeviceContext*    context          = nullptr;
        IDXGISwapChain*         swapChain        = nullptr;
        ID3D11RenderTargetView* renderTargetView = nullptr;
    };

    static HWND           g_hwnd     = nullptr;
    static SettingsDX     g_dx       = {};
    static ImGuiContext*  g_imguiCtx = nullptr;
    static bool           g_isOpen   = false;

    // ---- Example UI state -----------------------------------
    static bool  s_featureEnabled = false;
    static int   s_counter        = 0;
    static char  s_inputBuf[128]  = "Hello from settings!";

    // ---- D3D11 helpers --------------------------------------
    static bool DX_Init(HWND hwnd)
    {
        RECT rc = {};
        GetClientRect(hwnd, &rc);
        const int w = rc.right  - rc.left;
        const int h = rc.bottom - rc.top;

        DXGI_SWAP_CHAIN_DESC sd              = {};
        sd.BufferCount                       = 2;
        sd.BufferDesc.Width                  = (UINT)w;
        sd.BufferDesc.Height                 = (UINT)h;
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
            &sd, &g_dx.swapChain, &g_dx.device, &featureLevel, &g_dx.context);

        if (FAILED(hr)) return false;

        ID3D11Texture2D* pBB = nullptr;
        hr = g_dx.swapChain->GetBuffer(0, IID_PPV_ARGS(&pBB));
        if (FAILED(hr)) return false;

        hr = g_dx.device->CreateRenderTargetView(pBB, nullptr,
                                                 &g_dx.renderTargetView);
        pBB->Release();
        return SUCCEEDED(hr);
    }

    static void DX_CleanupRenderTarget()
    {
        if (g_dx.renderTargetView)
        {
            g_dx.renderTargetView->Release();
            g_dx.renderTargetView = nullptr;
        }
    }

    static void DX_CreateRenderTarget()
    {
        ID3D11Texture2D* pBB = nullptr;
        g_dx.swapChain->GetBuffer(0, IID_PPV_ARGS(&pBB));
        g_dx.device->CreateRenderTargetView(pBB, nullptr, &g_dx.renderTargetView);
        pBB->Release();
    }

    static void DX_Cleanup()
    {
        DX_CleanupRenderTarget();
        if (g_dx.swapChain) { g_dx.swapChain->Release(); g_dx.swapChain = nullptr; }
        if (g_dx.context)   { g_dx.context->Release();   g_dx.context   = nullptr; }
        if (g_dx.device)    { g_dx.device->Release();    g_dx.device    = nullptr; }
    }

    // ---- Window procedure -----------------------------------
    static LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg,
                                            WPARAM wParam, LPARAM lParam)
    {
        // Let ImGui process input first
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
            return TRUE;

        switch (msg)
        {
        // Resize: rebuild swap-chain buffers
        case WM_SIZE:
            if (g_dx.swapChain && wParam != SIZE_MINIMIZED)
            {
                DX_CleanupRenderTarget();
                g_dx.swapChain->ResizeBuffers(
                    0, LOWORD(lParam), HIWORD(lParam),
                    DXGI_FORMAT_UNKNOWN, 0);
                DX_CreateRenderTarget();
            }
            return 0;

        // Clicking "X" closes the settings window but keeps the overlay alive
        case WM_CLOSE:
            Settings_Close();
            return 0;

        case WM_DESTROY:
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
} // anonymous namespace

// ============================================================
//  Public API
// ============================================================

bool Settings_Open(HINSTANCE hInstance)
{
    if (g_isOpen && g_hwnd)
    {
        // Already open → bring to front
        ShowWindow(g_hwnd, SW_RESTORE);
        SetForegroundWindow(g_hwnd);
        return true;
    }

    // ---- Register window class (once) -----------------------
    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXW wc   = {};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = SettingsWndProc;
        wc.hInstance     = hInstance;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"SettingsWndClass";
        RegisterClassExW(&wc);
        classRegistered = true;
    }

    // ---- Size and center ------------------------------------
    const int winW = 480;
    const int winH = 340;
    const int scrW = GetSystemMetrics(SM_CXSCREEN);
    const int scrH = GetSystemMetrics(SM_CYSCREEN);
    const int posX = (scrW - winW) / 2;
    const int posY = (scrH - winH) / 2;

    // ---- Create window --------------------------------------
    g_hwnd = CreateWindowExW(
        0,
        L"SettingsWndClass",
        L"Overlay Settings",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME, // fixed size
        posX, posY, winW, winH,
        nullptr, nullptr, hInstance, nullptr);

    if (!g_hwnd) return false;

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    // ---- Init D3D11 -----------------------------------------
    if (!DX_Init(g_hwnd))
    {
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
        return false;
    }

    // ---- Init ImGui (separate context for this window) ------
    g_imguiCtx = ImGui::CreateContext();
    ImGui::SetCurrentContext(g_imguiCtx);

    ImGuiIO& io  = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowRounding = 4.0f;
    ImGui::GetStyle().FrameRounding  = 3.0f;

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_dx.device, g_dx.context);

    g_isOpen = true;
    return true;
}

void Settings_Close()
{
    if (!g_isOpen) return;

    g_isOpen = false;   // mark closed first so RenderFrame() is no-op

    // Shutdown ImGui for this window
    if (g_imguiCtx)
    {
        ImGui::SetCurrentContext(g_imguiCtx);
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(g_imguiCtx);
        g_imguiCtx = nullptr;
    }

    DX_Cleanup();

    if (g_hwnd)
    {
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
    }
}

bool Settings_IsOpen()
{
    return g_isOpen;
}

void Settings_RenderFrame()
{
    if (!g_isOpen || !g_hwnd || !g_imguiCtx) return;

    ImGui::SetCurrentContext(g_imguiCtx);

    // ---- Begin ImGui frame ----------------------------------
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // ---- Build settings UI ----------------------------------
    ImGuiIO& io = ImGui::GetIO();

    // Full-window docked panel
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);

    ImGuiWindowFlags panelFlags =
        ImGuiWindowFlags_NoTitleBar  |
        ImGuiWindowFlags_NoResize    |
        ImGuiWindowFlags_NoMove      |
        ImGuiWindowFlags_NoCollapse  |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("Settings Panel", nullptr, panelFlags);

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
                       " Overlay App – Settings");
    ImGui::Separator();
    ImGui::Spacing();

    // ---- Example controls -----------------------------------
    ImGui::PushItemWidth(240.0f);

    ImGui::Checkbox("Enable feature", &s_featureEnabled);
    ImGui::SameLine();
    ImGui::TextDisabled("(example checkbox)");

    ImGui::Spacing();

    if (s_featureEnabled)
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f),
                           "Feature is ON");
    else
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                           "Feature is OFF");

    ImGui::Spacing();
    ImGui::InputText("Label", s_inputBuf, sizeof(s_inputBuf));

    ImGui::Spacing();
    ImGui::Text("Counter: %d", s_counter);
    ImGui::SameLine();
    if (ImGui::Button("+1"))  ++s_counter;
    ImGui::SameLine();
    if (ImGui::Button("Reset")) s_counter = 0;

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Overlay keeps running even when this window is closed.");
    ImGui::TextDisabled("FPS: %.1f", io.Framerate);

    ImGui::PopItemWidth();

    ImGui::End();

    // ---- Render ---------------------------------------------
    ImGui::Render();

    const float clear[4] = { 0.12f, 0.12f, 0.14f, 1.0f };
    g_dx.context->OMSetRenderTargets(1, &g_dx.renderTargetView, nullptr);
    g_dx.context->ClearRenderTargetView(g_dx.renderTargetView, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    g_dx.swapChain->Present(1, 0);
}

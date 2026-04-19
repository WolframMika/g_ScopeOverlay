#pragma once

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#include <dxgi1_2.h>
#pragma comment(lib, "dxgi.lib")
#include <cmath>
#include <algorithm>
#include <string>

#define IMGUI_DEFINE_MATH_OPERATORS

namespace gBackend
{
	// Backend Data
	extern ID3D11Device* pd3dDevice;
	extern ID3D11DeviceContext* pd3dDeviceContext;
	extern IDXGISwapChain* pSwapChain;
	extern bool SwapChainOccluded;
	extern UINT ResizeWidth, ResizeHeight;
	extern ID3D11RenderTargetView* mainRenderTargetView;

	// Forward declarations of helper functions
	bool CreateDeviceD3D(HWND hWnd);
	void CleanupDeviceD3D();
	void CreateRenderTarget();
	void CleanupRenderTarget();
}
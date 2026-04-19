#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>
#pragma comment(lib, "dxgi.lib")

struct gSizePos {
	unsigned int dx, dy;
	unsigned int x, y;
};

class gCapture
{
private:
	IDXGIOutputDuplication* _deskDupl = nullptr;
	ID3D11Texture2D* _captureTex = nullptr;
	ID3D11ShaderResourceView* _captureTexSRV = nullptr;
	gSizePos _sizepos = {}, _last_sizepos = {};
	ID3D11Device* _pd3dDevice = nullptr;
	ID3D11DeviceContext* _pd3dDeviceContext = nullptr;

	void InitDesktopDuplication();
	void CleanupCaptureResources();
	void CaptureScreenRegion();

public:
	gCapture(gSizePos sizepos, ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dDeviceContext);

	void capture(gSizePos sizepos);

	ID3D11ShaderResourceView* get();

	~gCapture();
};
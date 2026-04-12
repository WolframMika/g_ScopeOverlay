#include "gCapture.h"

void gCapture::InitDesktopDuplication()
{
	IDXGIDevice* dxgiDevice = nullptr;
	IDXGIAdapter* dxgiAdapter = nullptr;
	IDXGIOutput* dxgiOutput = nullptr;
	IDXGIOutput1* dxgiOutput1 = nullptr;

	_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
	dxgiDevice->GetAdapter(&dxgiAdapter);
	dxgiAdapter->EnumOutputs(0, &dxgiOutput);
	dxgiOutput->QueryInterface(__uuidof(IDXGIOutput1), (void**)&dxgiOutput1);
	dxgiOutput1->DuplicateOutput(_pd3dDevice, &_deskDupl);

	dxgiOutput1->Release(); dxgiOutput->Release();
	dxgiAdapter->Release(); dxgiDevice->Release();
}

void gCapture::CleanupCaptureResources()
{
	if (_captureTexSRV) { _captureTexSRV->Release(); _captureTexSRV = nullptr; }
	if (_captureTex) { _captureTex->Release();    _captureTex = nullptr; }
	if (_deskDupl) { _deskDupl->Release();      _deskDupl = nullptr; }
}

void gCapture::CaptureScreenRegion()
{
	if (!_deskDupl) return;

	IDXGIResource* deskRes = nullptr;
	DXGI_OUTDUPL_FRAME_INFO frameInfo = {};

	HRESULT hr = _deskDupl->AcquireNextFrame(0, &frameInfo, &deskRes);
	if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_INVALID_CALL)
	{
		// Duplication interface is stale — rebuild it
		if (_deskDupl) { _deskDupl->Release(); _deskDupl = nullptr; }
		InitDesktopDuplication();
		return;
	}
	if (FAILED(hr)) return; // DXGI_ERROR_WAIT_TIMEOUT etc. — no new frame, keep last

	ID3D11Texture2D* deskTex = nullptr;
	deskRes->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&deskTex);

	// Recreate our texture only when size changes
	if (!_captureTex || _last_sizepos.dx != _sizepos.dx || _last_sizepos.dy != _sizepos.dy)
	{
		if (_captureTexSRV) { _captureTexSRV->Release(); _captureTexSRV = nullptr; }
		if (_captureTex) { _captureTex->Release();    _captureTex = nullptr; }

		D3D11_TEXTURE2D_DESC td = {};
		td.Width = _sizepos.dx; td.Height = _sizepos.dy;
		td.MipLevels = 1; td.ArraySize = 1;
		td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		_pd3dDevice->CreateTexture2D(&td, nullptr, &_captureTex);
		_pd3dDevice->CreateShaderResourceView(_captureTex, nullptr, &_captureTexSRV);
		_last_sizepos.dx = _sizepos.dx; _last_sizepos.dy = _sizepos.dy;
	}

	// GPU-only region blit
	int screenW = ::GetSystemMetrics(SM_CXSCREEN);
	int screenH = ::GetSystemMetrics(SM_CYSCREEN);

	int left = max(0, _sizepos.x - (_sizepos.dx / 2));
	int top = max(0, _sizepos.y - (_sizepos.dy / 2));
	int right = min(screenW, _sizepos.x + (_sizepos.dx / 2));
	int bottom = min(screenH, _sizepos.y + (_sizepos.dy / 2));

	D3D11_BOX box = { 
		(UINT)left,
		(UINT)top, 0,
		(UINT)right,
		(UINT)bottom, 1
	};

	_pd3dDeviceContext->CopySubresourceRegion(_captureTex, 0, 0, 0, 0, deskTex, 0, &box);

	deskTex->Release();
	deskRes->Release();
	_deskDupl->ReleaseFrame();
}

gCapture::gCapture(gSizePos sizepos, ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dDeviceContext) : _sizepos(sizepos), _pd3dDevice(pd3dDevice), _pd3dDeviceContext(pd3dDeviceContext) {
	InitDesktopDuplication();
}

void gCapture::capture(gSizePos sizepos) {
	_sizepos = sizepos;
	CaptureScreenRegion();
}

ID3D11ShaderResourceView* gCapture::get() {
	return _captureTexSRV;
}

gCapture::~gCapture() {
	CleanupCaptureResources();
}

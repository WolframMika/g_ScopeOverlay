#include "Render.h"

namespace Render {
	ImGuiIO* io = nullptr;
	ImGuiStyle* style = nullptr;
	gCapture* screen_capture = nullptr;
	HWND hwnd;
	int screenW;
	int screenH;
	unsigned int g_mode = 0;
}

void Render::render() {
	// Start the Dear ImGui frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Fullscreen transparent non-interactive background
	{
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(io->DisplaySize);
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::Begin("##overlay_bg", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);


		if (Config::ShowFpsOverlay) {
			ImVec2 pos1 = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(pos1.x + 60, pos1.y + 60));
			ImGui::PushFont(NULL, style->FontSizeBase * 1.5);

			ImVec2 pos = ImGui::GetCursorScreenPos();

			char text[32];
			snprintf(text, sizeof(text), "%.1f FPS", io->Framerate);

			ImVec2 textSize = ImGui::CalcTextSize(text);
			ImVec2 padding = ImVec2(4, 2);

			ImVec2 bg_min = pos;
			ImVec2 bg_max = ImVec2(pos.x + textSize.x + padding.x * 2,
				pos.y + textSize.y + padding.y * 2);

			ImGui::GetWindowDrawList()->AddRectFilled(
				bg_min,
				bg_max,
				IM_COL32(64, 64, 64, 210),
				4.0f
			);

			pos1 = ImGui::GetCursorPos();
			ImGui::SetCursorPos(ImVec2(pos1.x + padding.x, pos1.y + padding.y));
			FpsMarker(io->Framerate);
			ImGui::PopFont();
		}

		if (Config::ShowZoomOverlay) {

			screen_capture->capture({ (unsigned int)(Config::OutW / Config::Zoom), (unsigned int)(Config::OutH / Config::Zoom), (unsigned int)Config::PosX, (unsigned int)Config::PosY });

			ID3D11ShaderResourceView* captureTexSRV = screen_capture->get();

			if (captureTexSRV)
			{
				ImVec2 pos = ImVec2((float)Config::PosX - (Config::OutW / 2), (float)Config::PosY - (Config::OutH / 2));
				ImVec2 size = ImVec2(Config::OutW, Config::OutH);

				ImDrawList* draw_list = ImGui::GetWindowDrawList();
				draw_list->AddImageRounded(
					(ImTextureID)captureTexSRV,
					pos,
					ImVec2(pos.x + size.x, pos.y + size.y),
					ImVec2(0, 0),
					ImVec2(1, 1),
					IM_COL32_WHITE,
					Config::Rounding
				);
			}
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
			FpsMarker(io->Framerate);
			ImGui::Checkbox("FPS Overlay", &Config::ShowFpsOverlay);

			break;

		case 2:
			ImGui::Text("Keybind settings");

			if (ImGui::Button("Set Keybind"))
			{
				g_zoom_keybind.change = true;
			}

			ImGui::Text("Key: %s", (g_zoom_keybind.change ? "Press a key..." : GetKeyName(g_zoom_keybind.key)));

			ImGui::Checkbox("Is Toggle Zoom", &Config::ToggleToZoom);

			break;

		default:

			unsigned int step = 1;
			unsigned int step_fast = 10;
			float max_round = min((Config::OutW) / 2, (Config::OutH) / 2);

			ImGui::Text("Capture Region");

			ImGui::SameLine(); HelpMarker("Position is in the center of capture region. \n Hold Ctrl to adjust faster with -/+ & to type in slider.");

			ImGui::SeparatorText("Position");

			if (ImGui::InputScalar("x", ImGuiDataType_U32, &Config::PosX, &step, &step_fast)) {
				Config::PosX = min(screenW - Config::OutW / 2, max(0 + Config::OutW / 2, Config::PosX));
			}

			if (ImGui::InputScalar("y", ImGuiDataType_U32, &Config::PosY, &step, &step_fast)) {
				Config::PosY = min(screenH - Config::OutH / 2, max(0 + Config::OutH / 2, Config::PosY));
			}

			if (ImGui::Button("Center to screen")) {
				Config::PosX = (screenW) / 2;
				Config::PosY = (screenH) / 2;
			}

			ImGui::SeparatorText("Size");

			if (ImGui::InputScalar("dx", ImGuiDataType_U32, &Config::OutW, &step, &step_fast)) {
				Config::OutW = min(screenW, Config::OutW);
				Config::PosX = min(screenW - Config::OutW / 2, max(0 + Config::OutW / 2, Config::PosX));
				Config::Rounding = min(max_round, Config::Rounding);
			}

			if (ImGui::InputScalar("dy", ImGuiDataType_U32, &Config::OutH, &step, &step_fast)) {
				Config::OutH = min(screenH, Config::OutH);
				Config::PosY = min(screenH - Config::OutH / 2, max(0 + Config::OutH / 2, Config::PosY));
				Config::Rounding = min(max_round, Config::Rounding);
			}

			ImGui::SeparatorText("Appearance");

			ImGui::SliderFloat("zoom", &Config::Zoom, 1.0f, 8.0f, "%.1fx");

			if (ImGui::SliderFloat("roundness", &Config::Rounding, 0.0f, max_round, "%.1fx")) {
				Config::Rounding = min(max_round, Config::Rounding);
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

	HRESULT hr = gBackend::pSwapChain->Present(Config::Vsync, 0); //vsync
	gBackend::SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
}

// Helper to display a little (?) mark which shows a tooltip when hovered.
// In your own code you may want to display an actual icon if you are using a merged icon fonts (see docs/FONTS.md)
static void Render::HelpMarker(const char* desc)
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
static void Render::FpsMarker(float Framerate)
{
	if (Framerate < 15)
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%.1f FPS", Framerate);
	else if (Framerate < 60)
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%.1f FPS", Framerate);
	else
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%.1f FPS", Framerate);
}

// Helper to get key name from virtual key code
const char* Render::GetKeyName(int vk)
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
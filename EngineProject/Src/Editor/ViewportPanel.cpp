#include "EnginePCH.h"
#include "ViewportPanel.h"

#include "Render/RenderCommand.h"


bool FViewportPanel::Init()
{
	ValidateViewport();

	return false;
}

void FViewportPanel::Tick(float DeltaTime)
{
}

void FViewportPanel::OnRender()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 }); // 뷰포트 창 패딩 제거
	ImGui::Begin("Viewport", nullptr,
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoTitleBar
	);
	ImVec2 ImageMin = ImGui::GetCursorScreenPos();
	ViewportPos = ImageMin;

	AspectRatio = ViewportSize.x / ViewportSize.y;
	bIsHovered = ImGui::IsWindowHovered();
	ImVec2 CurrentViewportSize = ImGui::GetContentRegionAvail();
	if (CurrentViewportSize.x != ViewportSize.x || CurrentViewportSize.y != ViewportSize.y)
	{
		ViewportSize.x = CurrentViewportSize.x;
		ViewportSize.y = CurrentViewportSize.y;

		ValidateViewport();
	}
	ImGui::Image(ViewportFramebuffer->GetSRV(), ImVec2{ ViewportSize.x, ViewportSize.y });

	ImGui::End();
	ImGui::PopStyleVar();
}

void FViewportPanel::ValidateViewport()
{
	D3D11_TEXTURE2D_DESC Desc = {};
	Desc.Width = static_cast<UINT>(ViewportSize.x);
	Desc.Height = static_cast<UINT>(ViewportSize.y);
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	ViewportFramebuffer = RenderCommand::CreateTexture2D(Desc);

	Desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	DepthStencilBuffer = RenderCommand::CreateTexture2D(Desc);

	RenderingInfo.ColorRenderTargets.Reset();
	RenderingInfo.ViewportSetting.Width = ViewportSize.x;
	RenderingInfo.ViewportSetting.Height = ViewportSize.y;

	FRenderingDesc RenderingDesc{};
	RenderingDesc.Texture = ViewportFramebuffer.get();

	RenderingInfo.ColorRenderTargets.Add(RenderingDesc);
	RenderingInfo.DepthSteincil.Texture = DepthStencilBuffer.get();
}



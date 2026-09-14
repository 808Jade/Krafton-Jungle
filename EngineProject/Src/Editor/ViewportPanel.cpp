#include "EnginePCH.h"
#include "ViewportPanel.h"

#include "Render/RenderCommand.h"


bool FViewportPanel::Init()
{
	ViewportFramebuffer = RenderCommand::CreateRenderTargetTexture2D(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
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
	ValidateViewport();
	ImGui::Image(ViewportFramebuffer->GetSRV(), ImVec2{ ViewportSize.x, ViewportSize.y });

	ImGui::End();
	ImGui::PopStyleVar();
}

void FViewportPanel::ValidateViewport()
{
	ImVec2 CurrentViewportSize = ImGui::GetContentRegionAvail();

	if (CurrentViewportSize.x == ViewportSize.x && CurrentViewportSize.y == ViewportSize.y)return;

	ViewportSize.x = CurrentViewportSize.x;
	ViewportSize.y = CurrentViewportSize.y;

	ViewportFramebuffer = RenderCommand::CreateRenderTargetTexture2D(ViewportSize.x, ViewportSize.y, DXGI_FORMAT_R8G8B8A8_UNORM);

	RenderingInfo.ColorRenderTargets.Reset();
	RenderingInfo.ViewportSetting.Width = ViewportSize.x;
	RenderingInfo.ViewportSetting.Height = ViewportSize.y;

	FRenderTargetDesc Desc{};
	Desc.Texture = ViewportFramebuffer;

	RenderingInfo.ColorRenderTargets.Add(Desc);
}



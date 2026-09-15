#pragma once

#include "EditorPanel.h"
#include "Input/InputSystem.h"

class FViewportPanel : public IEditorPanel
{
public:
	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;

	bool IsHovered() const { return bIsHovered; }
	//뷰포트가 유효한지 확인
	void ValidateViewport();

	float GetViewportAspectRatio() const { return AspectRatio; }
	float GetWidth() const { return ViewportSize.x; }
	float GetHeight() const { return ViewportSize.y; }

	inline const FRenderingInfo& GetViewportRenderingInfo()const { return RenderingInfo; };

	FVector2 GetLocalMousePos() const
	{
		ImVec2 MousePos = ImGui::GetMousePos();
		return FVector2(MousePos.x - ViewportPos.x, MousePos.y - ViewportPos.y);
	}
private:
	bool bIsHovered = false;;
	ImVec2 ViewportSize = ImVec2{ 1,1 };
	ImVec2 ViewportPos;
	float AspectRatio = 0.0f;
	FRenderingInfo RenderingInfo{};
	TUniquePtr<FTexture2D> ViewportFramebuffer;
	TUniquePtr<FTexture2D> DepthStencilBuffer;
};


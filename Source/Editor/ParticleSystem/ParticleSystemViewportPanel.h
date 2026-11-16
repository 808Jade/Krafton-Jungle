#pragma once
#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/HitoriEd/EditorViewportClient.h"
#include "Render/RenderingInfo.h"

class FParticleEditorContext;
class FTexture2D;

// 파티클 에디터 ③ 뷰포트. 프리뷰 카메라와 렌더 타깃을 소유하고 결과 텍스처를 띄운다.
// 렌더러는 알지 못한다. 엔진이 이 패널의 View 상태·타깃과 Context의 프리뷰 월드로 RenderFrame을 호출한다.
class FParticleSystemViewportPanel : public IEditorPanel
{
public:
	explicit FParticleSystemViewportPanel(FParticleEditorContext& InContext) : Context(InContext) {}
	~FParticleSystemViewportPanel() override;

	bool Init() override;
	void Tick(float DeltaTime) override { LastDeltaTime = DeltaTime; }
	void OnRender() override;
	// ##Particle: 레벨 에디터의 같은 이름 창과 구분한다. 화면에는 앞부분만 보인다.
	const char* GetPanelName() const override { return "Viewport##Particle"; }

	// GPU 타깃을 해제한다. Device 종료 전에 호스트가 호출한다.
	void ReleaseResources();
	void ResetCamera();
	bool IsHoveredOrInteracting() const { return bWindowVisible && (bViewportHovered || bCameraCaptured); }

	// --- 엔진이 프리뷰를 그릴 때 쓰는 입력 ---
	// 뷰포트 크기에 맞춰 타깃을 준비한다. 이번 프레임에 그릴 수 없으면 false다.
	bool PreparePreviewRender();
	const FEditorViewportClient& GetViewportClient() const { return ViewportClient; }
	FRect GetViewRect() const { return { 0.0f, 0.0f, static_cast<float>(TargetWidth), static_cast<float>(TargetHeight) }; }
	const FRenderingInfo& GetRenderingInfo() const { return RenderingInfo; }
	FTexture2D* GetHDRTarget() const { return HDRTarget.get(); }
	FTexture2D* GetFxaaTarget() const { return FxaaTarget.get(); }

	// 프리뷰 배경색(RGB). 툴바의 색 선택기가 직접 고친다. 다음 렌더에서 지우기 색으로 반영된다.
	float* GetBackgroundColor() { return BackgroundColor; }

private:
	// 뷰포트 위의 우클릭 드래그·WASD·휠을 View 카메라에 전달한다. 레벨 뷰포트와 같은 조작이다.
	void UpdateCameraInput();
	void ResizeTargets(uint32 Width, uint32 Height);

	// 참조(집약): 호스트가 소유하는 공유 상태
	FParticleEditorContext& Context;

	// 소유(합성): 이 뷰포트의 카메라·ViewMode·ShowFlags. 레벨 View의 Client와 같은 클래스다.
	FEditorViewportClient ViewportClient;

	// 소유(합성): 프리뷰 결과를 받는 타깃. ColorTarget을 ImGui::Image로 띄운다.
	TUniquePtr<FTexture2D> ColorTarget;
	TUniquePtr<FTexture2D> DepthTarget;
	TUniquePtr<FTexture2D> HDRTarget;
	TUniquePtr<FTexture2D> FxaaTarget;
	FRenderingInfo RenderingInfo{};
	// 스카이박스 없이 그리므로 배경은 이 지우기 색이다.
	float BackgroundColor[3] = { 0.08f, 0.08f, 0.09f };
	uint32 TargetWidth = 0;
	uint32 TargetHeight = 0;
	// OnRender에서 측정한 영역 크기. 다음 프레임 렌더 전에 타깃 크기로 반영한다.
	uint32 RequestedWidth = 0;
	uint32 RequestedHeight = 0;

	float LastDeltaTime = 0.0f;
	bool bWindowVisible = false;
	bool bViewportHovered = false;
	bool bCameraCaptured = false;
};

#pragma once

#include "Editor/HitoriEd/EditorViewportClient.h"
#include "Editor/LevelViewportLayout.h"

#include "Collision/Ray.h"
#include "Math/Matrix.h"
#include "Render/RenderingInfo.h"

class UWorld;
class FRenderQueue;
class FTexture2D;
struct FLODViewContext;

// 그림자 작업을 View별로 구분하는 ID. 0은 World 공유 작업에 남겨 둔다.
// 레벨 View는 1~4, 별도 PIE 창은 5, 에셋 에디터 프리뷰는 6이다. (프리뷰 월드는 그림자 작업을 만들지 않아 같은 ID를 써도 된다)
inline constexpr uint32 ExternalPIEShadowViewId = FLevelViewportLayout::ViewCount + 1;
inline constexpr uint32 PreviewShadowViewId = ExternalPIEShadowViewId + 1;

// 화면의 View 하나. 카메라·표시 상태(Client), 화면 영역, 렌더 타깃, 그림자 View ID를 가진다.
// (Week11의 FViewport와 같은 역할. 예전 Adapter의 "인덱스로 고르는 View"를 객체 하나로 바꾼 것이다)
// 화면 안의 배치는 이 View를 담은 패널·창이 정하고 SetRect로 넘겨준다.
class FViewport
{
public:
	explicit FViewport(uint32 InShadowViewId = 0) : ShadowViewId(InShadowViewId) {}
	~FViewport() = default;
	FViewport(const FViewport&) = delete;
	FViewport& operator=(const FViewport&) = delete;

	FEditorViewportClient& GetClient() { return Client; }
	const FEditorViewportClient& GetClient() const { return Client; }
	// 렌더·컬링·피킹에 쓰는 카메라. PIE 카메라 Override가 있으면 그쪽이다.
	const FViewCamera& GetEffectiveCamera() const { return Client.GetEffectiveCamera(); }

	uint32 GetShadowViewId() const { return ShadowViewId; }
	void SetShadowViewId(uint32 Value) { ShadowViewId = Value; }

	// 담은 패널 안에서의 영역(로컬 좌표). 크기가 0이거나 bInActive가 false면 비활성이다.
	// 크기가 바뀌면 직교 폭을 보정하고, 렌더 타깃을 소유하는 View는 타깃도 다시 만든다.
	void SetRect(const FRect& InRect, bool bInActive);
	const FRect& GetRect() const { return Rect; }
	bool IsActive() const { return bActive; }
	// 패널 로컬 좌표를 이 View 기준 좌표로 바꾼다.
	FVector2 ToViewLocal(FVector2 PanelLocal) const { return { PanelLocal.X - Rect.X, PanelLocal.Y - Rect.Y }; }

	// --- 렌더 타깃 ---
	// true면 SetRect가 크기에 맞춰 Color·Depth·HDR·FXAA 타깃을 직접 만든다. (레벨 View)
	// false면 SetExternalTargets로 받은 타깃을 쓴다. (별도 PIE 창: 창의 Swapchain 쪽 타깃)
	void SetOwnsRenderTargets(bool bOwns) { bOwnsRenderTargets = bOwns; }
	void SetExternalTargets(const FRenderingInfo* InRenderingInfo, FTexture2D* InHDRTarget, FTexture2D* InFxaaTarget);
	const FRenderingInfo* GetRenderingInfo() const;
	FTexture2D* GetColorTarget() const { return ColorTarget.get(); }
	FTexture2D* GetHDRTarget() const;
	FTexture2D* GetFxaaTarget() const;
	void ReleaseRenderTargets();

	// --- 카메라 계산 (예전 Adapter의 인덱스 함수와 같은 결과) ---
	FMatrix GetViewProjection() const;
	FMatrix GetViewMatrix() const;
	FMatrix GetProjectionMatrix() const;
	FLODViewContext GetLODViewContext() const;
	bool AppendShadowFrustumCorners(float ShadowDistance, TArray<FVector>& OutCorners) const;
	bool AppendShadowFrustumCorners(float NearDistance, float FarDistance, TArray<FVector>& OutCorners) const;
	FVector GetCameraLocation() const { return GetEffectiveCamera().Transform.Location; }
	FVector GetCameraForward() const;
	FVector GetCameraUp() const;
	FMatrix BuildBillboardMatrix(const FVector& WorldPosition, float Width, float Height) const;
	bool IsOrthographic() const { return GetEffectiveCamera().Projection.Mode == EProjectionMode::Orthographic; }
	// View 기준 좌표를 렌더 카메라로 역투영한다.
	FRay Deproject(FVector2 ViewLocalPosition) const;

	// 이 View의 카메라·영역으로 컬링·LOD 입력을 만들어 World의 공통 수집 경로를 호출한다.
	void BuildRenderQueue(UWorld& World, FRenderQueue& OutQueue, FRenderQueue& OutIconQueue, FRenderQueue& OutArrowQueue) const;
	// FViewport를 쓰지 않는 호출자(애셋 에디터 프리뷰 등)를 위한 같은 경로
	static void BuildRenderQueue(UWorld& World, const FViewCamera& ViewCamera, const FRect& ViewRect,
		FRenderQueue& OutQueue, FRenderQueue& OutIconQueue, FRenderQueue& OutArrowQueue);

private:
	void ResizeTargets(uint32 Width, uint32 Height);

	// 소유(합성): 이 View의 카메라·프리셋·ViewMode·ShowFlags
	FEditorViewportClient Client;
	FRect Rect{};
	bool bActive = false;
	uint32 ShadowViewId = 0;

	bool bOwnsRenderTargets = false;
	// 소유(합성): bOwnsRenderTargets일 때만 만든다.
	TUniquePtr<FTexture2D> ColorTarget;
	TUniquePtr<FTexture2D> DepthTarget;
	TUniquePtr<FTexture2D> FxaaTarget;
	TUniquePtr<FTexture2D> HDRTarget;
	FRenderingInfo RenderingInfo{};
	uint32 TargetWidth = 0;
	uint32 TargetHeight = 0;

	// 참조(집약): 외부가 소유하는 타깃 (별도 PIE 창)
	const FRenderingInfo* ExternalRenderingInfo = nullptr;
	FTexture2D* ExternalHDRTarget = nullptr;
	FTexture2D* ExternalFxaaTarget = nullptr;
};

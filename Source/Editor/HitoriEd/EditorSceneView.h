#pragma once

#include "Editor/HitoriEd/EditorViewportClient.h"

class FTexture2D;
class FGizmo;
class FBoneWeightHeatmap;
struct FSkeletonDrawParams;

// View의 에디터 그리드 표시 방식
enum class EViewGridMode : uint8
{
	FollowOverlay,	// bShowEditorOverlay와 설정 패널의 Grid 옵션을 따른다. (레벨 View)
	Show,			// 오버레이와 상관없이 그린다. (프리뷰)
	Hide,
};

// RenderFrame이 View 하나를 그리는 데 필요한 입력을 모은다. (언리얼의 FSceneViewFamily에 해당)
// 레벨 뷰포트·PIE 창·애셋 에디터 프리뷰가 각자 이 구조체를 채워 같은 RenderFrame을 호출한다.
// 모든 포인터는 참조만 하며, RenderFrame 호출 동안만 유효하면 된다.
struct FEditorSceneView
{
	// 카메라·ViewMode·ShowFlags·프리셋을 가진 View 상태. 소유자는 Adapter 또는 프리뷰 패널이다.
	const FEditorViewportClient* Client = nullptr;
	// 렌더 영역. 투영의 Aspect와 직교 깊이 보정에 쓴다.
	FRect Rect{};
	// 장면을 먼저 그리는 HDR 임시 타깃과 FXAA 임시 타깃. FxaaTarget이 없으면 FXAA를 건너뛴다.
	FTexture2D* HDRTarget = nullptr;
	FTexture2D* FxaaTarget = nullptr;
	// Directional 그림자 작업을 View별로 구분하는 ID. 0은 World 공유 작업용이다.
	uint32 ShadowViewId = 0;
	// Grid·Gizmo·Outline·아이콘 같은 에디터 표시를 그릴지 여부
	bool bShowEditorOverlay = true;
	bool bDrawSkybox = true;
	// 게임 UI(UIRenderer)를 이 View 위에 그릴지 여부
	bool bRenderGameUI = false;

	// --- 애셋 에디터 프리뷰가 View별로 정하는 표시 (Week11의 FEditorTabView 확장에 해당). 레벨 View는 기본값을 쓴다 ---
	// 에디터 그리드. 프리뷰는 아이콘·Outline 같은 오버레이 없이 그리드만 켤 수 있다.
	EViewGridMode GridMode = EViewGridMode::FollowOverlay;
	// 0 이상이면 설정 패널의 노출 대신 이 값을 쓴다.
	float ExposureOverride = -1.0f;
	// 있으면 본(뼈대)을 오버레이로 그린다. 본 행렬은 소유자가 이번 프레임 값으로 채운다.
	const FSkeletonDrawParams* Skeleton = nullptr;
	// 프리뷰가 따로 쓰는 Gizmo(본 Gizmo). 대상이 있을 때만 그린다.
	const FGizmo* PreviewGizmo = nullptr;
	// 본 웨이트 히트맵. Status가 On이면 장면 패스 동안 웨이트 버퍼를 바인드한다.
	const FBoneWeightHeatmap* BoneWeightHeatmap = nullptr;
	uint32 BoneWeightHeatmapStatus = 0;

	bool IsOrthographic() const
	{
		return Client->GetEffectiveCamera().Projection.Mode == EProjectionMode::Orthographic;
	}
	FMatrix CalcView() const
	{
		FMatrix ViewMatrix, Projection;
		FEditorViewportClient::CalcViewMatrices(Client->GetEffectiveCamera(), Rect, ViewMatrix, Projection);
		return ViewMatrix;
	}
	FMatrix CalcProjection() const
	{
		FMatrix ViewMatrix, Projection;
		FEditorViewportClient::CalcViewMatrices(Client->GetEffectiveCamera(), Rect, ViewMatrix, Projection);
		return Projection;
	}
};

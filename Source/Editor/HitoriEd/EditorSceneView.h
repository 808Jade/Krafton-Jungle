#pragma once

#include "Editor/HitoriEd/EditorViewportClient.h"

class FTexture2D;

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

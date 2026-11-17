#pragma once
#include "Editor/Gizmo/Gizmo.h"
#include "Editor/Rendering/SkeletonRenderer.h"
#include "Math/Transform.h"

class FSkeletalPreviewTabBase;
class USceneComponent;
struct FSkeletalBone;

// 본 선택·Hover·뷰포트 피킹과 본 기즈모 편집. 트리·뷰포트·디테일 패널이 이 상태를 함께 본다.
// 본 표시 옵션(원뿔·선, 굵기, 이름 표시)과 엔진에 넘길 본 그리기 입력(SkeletonDraw)도 가진다.
class FBoneEditController
{
public:
	explicit FBoneEditController(FSkeletalPreviewTabBase& InTab) : Tab(InTab) {}

	// 프리뷰 컴포넌트의 현재 포즈로 본별 월드 행렬을 계산한다.
	void ComputeBoneWorldMatrices(TArray<FMatrix>& OutBoneWorldMatrices) const;
	// 뷰포트 Hover·클릭으로 본을 고르고, 기즈모 드래그를 본 Transform에 반영한다.
	void UpdateBoneInteraction();
	bool IsBoneAncestorOf(int32 AncestorIndex, int32 TargetBoneIndex, const TArray<FSkeletalBone>& Bones) const;
	FTransform GetBoneLocalTransform(int32 BoneIndex) const;
	void ResetAllBonesToBindPose();

	// 참조(집약): 기즈모가 붙는 임시 대상. 탭이 만들고 지운다. (본은 컴포넌트가 아니므로 대리 대상을 쓴다)
	USceneComponent* BoneGizmoTarget = nullptr;
	FGizmo BoneGizmo;

	// 본 트리 UI 및 피킹 상태
	int32 SelectedBoneIndex = -1;
	int32 HoveredBoneIndex = -1;
	bool bScrollToSelectedBone = false;

	// 골격(Bone) 표시 및 하이라이트 옵션
	bool bShowBones = true;
	bool bShowBoneGizmo = true;
	bool bDrawBoneCones = true;      // true: 입체 원뿔(Cone/Pyramid) 렌더링, false: 선(Line) 렌더링
	bool bShowAllBoneNames = false;  // 모든 본 이름 표시 여부 (선택된 본은 항상 표시)
	float BoneConeWidthRatio = 0.10f;
	float BoneJointRadius = 2.5f;

	// 이번 프레임에 엔진이 그릴 본. 탭의 CollectViews가 채우고 View가 가리킨다.
	FSkeletonDrawParams SkeletonDraw;

private:
	// 참조(집약): 이 객체를 가진 탭. 메시·프리뷰 컴포넌트·카메라·뷰포트 영역을 읽는다. 탭이 이 객체보다 오래 산다.
	FSkeletalPreviewTabBase& Tab;
};

#pragma once

#include "Editor/EditorUI/EditorTab.h"
#include "Editor/SkeletalMeshViewer/SkeletalPreviewCamera.h"
#include "Editor/SkeletalMeshViewer/BoneEditController.h"
#include "Editor/SkeletalMeshViewer/SkeletalAnimationPreview.h"
#include "Editor/SkeletalMeshViewer/SkeletalNotifyEditor.h"
#include "Editor/SkeletalMeshViewer/SkeletalMeshAssetSaver.h"
#include "Editor/Rendering/SkeletonRenderer.h"
#include "Editor/Viewports/Viewport.h"
#include "PreviewScene.h"
#include "Render/Mesh.h"
#include "Render/ViewMode.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/EngineString.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "BoneWeightHeatmap.h"
#include <functional>
#include <utility>
#include "imgui.h"

class UAnimSequence;
class UAnimSingleNodeInstance;
class UAnimDataModel;
class UBlendSpace1D;
class ASkeletalMeshActor;
class ADirectionalLightActor;
class ASkyLightActor;
class AStaticMeshActor;

struct FAnimStateMachineAssetData;
class UAssetAnimInstance;
class UAnimGraphInstance;
struct FAnimGraphAssetData;

// 스켈레탈 메시 프리뷰 탭의 공통 부분. 
// 탭은 프리뷰 월드·선택한 메시·조명·표시 설정과 View를 가지고, 나머지 편집 기능은 책임별 객체로 나눠 소유한다.
//  - Camera     : 카메라와 뷰포트 조작          - BoneEdit   : 본 선택·피킹·기즈모 편집
//  - Animation  : 애니메이션 미리보기 재생        - NotifyEdit : 노티파이 편집
//  - AssetSaver : 본 포즈를 메시 애셋에 저장·복원
// 하위 패널(뷰포트·본 트리·디테일 등)은 이 탭을 참조해 상태를 읽고 쓴다.
// 렌더러는 모른다. 프리뷰 월드와 View 옵션만 CollectViews로 넘기고, 그리기는 엔진의 RenderFrame이 한다.
// 패널·툴바·배치는 파생 클래스(FSkeletalMeshViewerTab)가 붙인다.
// AnimGraph 에디터는 이 클래스를 화면에 띄우지 않은 채 문서별 미리보기로 쓴다. (CreateAnimationPreviewBackend)
class FSkeletalPreviewTabBase : public FEditorTab
{
public:
	explicit FSkeletalPreviewTabBase(const FEditorTabSettings& InSettings);
	~FSkeletalPreviewTabBase() override;

	// 프리뷰 월드와 그 안의 메시·라이트를 만든다.
	bool Init() override;
	// 재생·입력·카메라를 갱신한다. 엔진은 보이는 탭만 부른다. (임베디드 미리보기는 소유자가 부른다)
	void Tick(float DeltaTime) override;
	// 프리뷰 월드와 View 옵션(본·기즈모·히트맵·노출·그리드)을 엔진에 넘긴다. 뷰포트가 보일 때만 넘긴다.
	void CollectViews(TArray<FEditorTabView>& OutViews) override;
	void ReleaseResources() override;
	// 뷰어 패널이 마우스 조작 중이거나 호버 상태인지 확인 (메인 에디터 뷰포트 입력 간섭 차단용)
	bool IsHoveredOrInteracting() const override { return bViewportHovered || bIsInteracting; }

	// AnimGraph 문서별 미리보기용. 화면(EditorUI)에 붙이지 않는 같은 클래스의 인스턴스를 만든다.
	std::unique_ptr<FSkeletalPreviewTabBase> CreateAnimationPreviewBackend() const;
	void DrawEmbeddedPreview();
	void HideEmbeddedPreview() { bViewportVisible = false; bViewportHovered = false; bViewportFocused = false; }

	// 탭이 매 프레임 그리기 전에 부른다. 뷰포트 표시·Hover·포커스 상태를 지운다. (가려진 탭의 뷰포트가 계속 렌더·입력을 받지 않게)
	void BeginFrame();
	// Mesh 메뉴에서 다른 메시를 고르면 이 함수로 넘긴다. (엔진이 그 메시의 탭을 연다) 비어 있으면 이 탭에서 연다.
	void SetOpenMeshRequestHandler(std::function<void(USkeletalMesh*)> Handler) { OnOpenMeshRequested = std::move(Handler); }
	USkeletalMesh* GetSelectedMesh() const { return SelectedMesh; }

	// 다중 애셋 탭 오픈 및 선택
	bool OpenAsset(const FString& Path);
	void OpenMesh(USkeletalMesh* Mesh);
	void SetSelectedMesh(USkeletalMesh* Mesh);
	void ResetLighting();

	void ResizeViewport(uint32 NewWidth, uint32 NewHeight);
	// 편집 상태(조명·표시 파츠·카메라·표시 모드)를 프리뷰 월드와 View에 반영한다. CollectViews 직전에 부른다.
	void SyncPreviewWorld();
	// 메시가 바뀌면 FBX의 정적 파츠 액터를 새 메시 기준으로 다시 만든다.
	void RebuildStaticPartActors();
	FBox CalculatePreviewBounds() const;
	UMaterial* GetPreviewMaterial(int32 SlotIndex) const;
	void SetPreviewSampler(int32 SlotIndex, ESamplerState Sampler);

	// 뷰어 표시 모드. View Mode 메뉴의 라디오 그룹(하나만 선택)이며, 체크 표시는 이 값 하나로 계산한다.
	// 엔진 View의 EViewMode에는 히트맵이 없으므로 GetRendererViewMode()로 변환해 넘긴다. (톤맵 방식은 엔진이 ViewMode로 고른다)
	enum class EViewerDisplayMode : uint8
	{
		LitBlinnPhong,
		LitLambert,
		Unlit,
		WorldNormal,
		BoneWeightHeatmap,
	};
	EViewMode GetRendererViewMode() const;
	bool IsBoneWeightHeatmapMode() const { return DisplayMode == EViewerDisplayMode::BoneWeightHeatmap; }

	// --- 책임별 편집 기능. 소유(합성): 탭 하나에만 속하고 탭과 수명이 같다. 각 객체는 탭을 참조해 공유 상태를 읽는다. ---
	FSkeletalPreviewCamera Camera{ *this };
	FBoneEditController BoneEdit{ *this };
	FSkeletalAnimationPreview Animation{ *this };
	FSkeletalNotifyEditor NotifyEdit{ *this };
	FSkeletalMeshAssetSaver AssetSaver{ *this };

	// --- 아래는 패널과 위 객체들이 함께 읽고 쓰는 상태다. (탭이 공유 상태의 주인이므로 공개한다) ---
	std::function<void(USkeletalMesh*)> OnOpenMeshRequested;

	// 소유(합성): 프리뷰 전용 월드. 메시·라이트·FBX 정적 파츠가 이 월드의 액터다. 탭이 닫히면 함께 사라진다.
	TUniquePtr<FPreviewScene> PreviewScene;
	// 참조(집약): 프리뷰 월드가 소유하는 액터들
	ASkeletalMeshActor* MeshActor = nullptr;
	ADirectionalLightActor* DirectionalLightActor = nullptr;
	ASkyLightActor* SkyLightActor = nullptr;
	TArray<AStaticMeshActor*> StaticPartActors;
	// 액터를 지웠으면 월드를 한 번 Tick해 정리한다. (애니메이션은 직접 TickComponent로 돌리므로 월드는 평소에 Tick하지 않는다)
	bool bPreviewWorldNeedsTick = false;
	// 참조(집약): MeshActor의 컴포넌트
	USkeletalMeshComponent* PreviewComponent = nullptr;
	USkeletalMesh* SelectedMesh = nullptr;
	TArray<TUniquePtr<UMaterial>> PreviewMaterials;
	TArray<UStaticMesh*> VisibleStaticMeshes;
	FBox PreviewBounds{};
	float MeshRadius = 1.0f;

	// 다중 애셋 탭 목록 및 전환 대기 메시
	TArray<USkeletalMesh*> OpenedMeshes;
	USkeletalMesh* MeshToSelect = nullptr;
	FString OpenAssetMessage;

	// 소유(합성): 프리뷰 화면 하나. 카메라(Client)와 렌더 타깃을 가진다. 크기는 뷰포트 패널(또는 임베디드 미리보기)이 정한다.
	FViewport PreviewViewport{ PreviewShadowViewId };
	uint32 ViewportWidth = 0;
	uint32 ViewportHeight = 0;
	ImVec2 CachedViewportOrigin{ 0.0f, 0.0f };
	ImVec2 CachedViewportSize{ 0.0f, 0.0f };
	bool bViewportVisible = false;
	bool bEmbeddedAnimationPreview = false;
	bool bFrameRequested = false;
	bool bViewportHovered = false;
	bool bIsInteracting = false;
	bool bViewportFocused = false;
	bool bAnimationWindowFocused = false;
	bool bBlendSpaceWindowFocused = false;

	// 개별 도킹 서브 윈도우 표시 상태
	bool bViewportOpen = true;
	bool bSkeletonTreeOpen = true;
	bool bAssetDetailsOpen = true;
	bool bPreviewSettingsOpen = true;
	bool bAnimationOpen = true;
	bool bBlendSpaceOpen = true;
	bool bBlendSpaceDockInitialized = false;
	bool bAnimationWasVisible = false;
	bool bBlendSpaceWasVisible = false;
	bool bResetLayout = false;

	// 미리보기의 방향광과 하늘광 설정이다.
	bool bEnableDirectionalLight = true;
	float LightYaw = 45.0f;
	float LightPitch = 45.0f;
	FVector LightColor = FVector(1.0f, 1.0f, 1.0f);
	float LightIntensity = 1.8f;

	bool bEnableSkyLight = true;
	FVector SkyColor = FVector(0.6f, 0.7f, 0.85f);
	FVector GroundColor = FVector(0.25f, 0.25f, 0.28f);
	float SkyLightIntensity = 1.0f;
	float PreviewExposure = 1.0f;

	// 뷰 모드 및 렌더 옵션
	EViewerDisplayMode DisplayMode = EViewerDisplayMode::LitBlinnPhong;
	bool bShowGrid = true;
	bool bWireframe = false;   // 표시 모드와 독립 토글 (히트맵 위에도 겹쳐 그릴 수 있다)
	FVector4 BackgroundClearColor = FVector4(0.12f, 0.12f, 0.12f, 1.0f); // 언리얼 기본 프리뷰 배경색
	FBoneWeightHeatmap BoneWeightHeatmap;

	// 저장 상태 알림 메시지 및 타이머 (툴바에 잠깐 보인다)
	FString SaveStatusMessage;
	float SaveStatusTimer = 0.0f;
};

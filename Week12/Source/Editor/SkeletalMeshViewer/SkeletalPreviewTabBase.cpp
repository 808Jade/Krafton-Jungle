#include "EnginePCH.h"
#include "Animation/AssetAnimInstance.h"
#include "Animation/AnimGraph.h"
#include "SkeletalPreviewTabBase.h"
#include "SkeletalMeshEditorUtils.h"

using namespace SkeletalMeshEditorUtils;
#include "Asset/AssetManager.h"
#include "Camera/MinimalViewInfo.h"
#include "Collision/Ray.h"
#include "Engine/SceneManagement.h"
#include "GameFramework/Actor/SkeletalMeshActor.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "GameFramework/Actor/DirectionalLightActor.h"
#include "GameFramework/Actor/SkyLightActor.h"
#include "Engine/World.h"
#include "Input/InputSystem.h"
#include "Render/RenderCommand.h"
#include "Math/EngineMath.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "Render/Material.h"
#include "Render/RenderResourceManager.h"
#include "Components/SkinnedMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Core/Misc/FileHelper.h"
#include "Animation/AnimNotifyFile.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotify_CameraShake.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Engine/Engine.h"
#include "Audio/AudioSystem.h"
#include "Camera/Shake/CameraShakePattern.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectGlobals.h"
#include "Core/Misc/FileHelper.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>


FSkeletalPreviewTabBase::FSkeletalPreviewTabBase(const FEditorTabSettings& InSettings)
	: FEditorTab(InSettings)
{
	ResetLighting();
}

FSkeletalPreviewTabBase::~FSkeletalPreviewTabBase()
{
	ReleaseResources();
}

bool FSkeletalPreviewTabBase::Init()
{
	// 프리뷰 월드마다 이름이 달라야 한다. (같은 메시를 여러 탭·AnimGraph 문서가 동시에 미리 볼 수 있다)
	static int32 PreviewWorldCounter = 0;
	PreviewScene = MakeUnique<FPreviewScene>(FName("SkeletalPreview_" + std::to_string(PreviewWorldCounter++)));
	if (!PreviewScene->GetWorld())
		return false;
	MeshActor = PreviewScene->SpawnActor<ASkeletalMeshActor>("PreviewSkeletalMesh");
	DirectionalLightActor = PreviewScene->SpawnActor<ADirectionalLightActor>("PreviewDirectionalLight");
	SkyLightActor = PreviewScene->SpawnActor<ASkyLightActor>("PreviewSkyLight");
	if (!MeshActor || !DirectionalLightActor || !SkyLightActor)
		return false;
	// 그림자는 그리지 않는다. 프리뷰 View들은 그림자 View ID를 함께 쓰므로 그림자 작업을 만들지 않는 것이 전제다.
	DirectionalLightActor->GetLightComponent()->SetCastShadows(false);
	bPreviewWorldNeedsTick = true;

	PreviewComponent = MeshActor->GetSkeletalMeshComponent();
	PreviewComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Animation.PreviewNotifyHandle = PreviewComponent->OnAnimNotify.AddDynamic(&Animation, &FSkeletalAnimationPreview::HandlePreviewNotify);
	Animation.PreviewNotifyStateBeginHandle = PreviewComponent->OnAnimNotifyStateBegin.AddDynamic(&Animation, &FSkeletalAnimationPreview::HandlePreviewNotifyStateBegin);
	Animation.PreviewNotifyStateEndHandle = PreviewComponent->OnAnimNotifyStateEnd.AddDynamic(&Animation, &FSkeletalAnimationPreview::HandlePreviewNotifyStateEnd);
	BoneEdit.BoneGizmoTarget = NewObject<USceneComponent>();
	BoneEdit.BoneGizmoTarget->SetFlags(EObjectFlags::RF_Transient);
	BoneEdit.BoneGizmo.SetMode(EGizmoMode::Rotation);
	BoneEdit.BoneGizmo.SetSpace(EGizmoSpace::Local);

	// 기본 뷰포트 크기. 뷰포트 패널이 실제 크기로 바꾼다.
	PreviewViewport.SetOwnsRenderTargets(true);
	ResizeViewport(800, 600);

	return true;
}

bool FSkeletalPreviewTabBase::OpenAsset(const FString& Path)
{
	const FFbxAssetSet* Assets = UAssetManager::Get().LoadFbxAssets(Path);
	if (!Assets)
	{
		OpenAssetMessage = "Could not open FBX asset: " + Path;
		RequestFocus();
		return false;
	}
	if (Assets->SkeletalMesh)
	{
		OpenMesh(Assets->SkeletalMesh);
		return true;
	}

	OpenAssetMessage = "Imported " + std::to_string(Assets->StaticMeshes.Num()) +
		" static mesh assets. This FBX has no skeletal mesh to preview.";
	RequestFocus();
	return true;
}

void FSkeletalPreviewTabBase::ReleaseResources()
{
	Animation.ResetPreviewModes();
	BoneEdit.BoneGizmo.SetTarget(nullptr, false);
	Animation.ClearPreviewClips();
	Animation.SelectedClipIndex = -1;
	if (PreviewComponent)
	{
		PreviewComponent->OnAnimNotify.RemoveHandler(Animation.PreviewNotifyHandle);
		PreviewComponent->OnAnimNotifyStateBegin.RemoveHandler(Animation.PreviewNotifyStateBeginHandle);
		PreviewComponent->OnAnimNotifyStateEnd.RemoveHandler(Animation.PreviewNotifyStateEndHandle);
	}
	Animation.PreviewNotifyHandle = 0;
	Animation.PreviewNotifyStateBeginHandle = 0;
	Animation.PreviewNotifyStateEndHandle = 0;
	Animation.FiredNotifies.Empty();
	Camera.PreviewShake.reset();
	// 컴포넌트·액터는 프리뷰 월드가 소유한다. 월드를 놓으면 함께 사라진다.
	PreviewComponent = nullptr;
	MeshActor = nullptr;
	DirectionalLightActor = nullptr;
	SkyLightActor = nullptr;
	StaticPartActors.Reset();
	if (PreviewScene)
		PreviewScene->Release();
	PreviewScene.reset();
	delete BoneEdit.BoneGizmoTarget;
	BoneEdit.BoneGizmoTarget = nullptr;
	PreviewMaterials.Reset();
	VisibleStaticMeshes.Reset();
	OpenedMeshes.Reset();
	SelectedMesh = nullptr;
	MeshToSelect = nullptr;
	PreviewViewport.ReleaseRenderTargets();
}

void FSkeletalPreviewTabBase::OpenMesh(USkeletalMesh* Mesh)
{
	if (!Mesh)
		return;

	OpenAssetMessage.clear();
	RequestFocus();

	// 이미 열려 있는지 검사
	bool bFound = false;
	for (USkeletalMesh* Opened : OpenedMeshes)
	{
		if (Opened == Mesh)
		{
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		OpenedMeshes.Add(Mesh);
	}

	MeshToSelect = Mesh;
	SetSelectedMesh(Mesh);
}

void FSkeletalPreviewTabBase::SetSelectedMesh(USkeletalMesh* Mesh)
{
	Animation.ResetPreviewModes();
	if (SelectedMesh != Mesh)
	{
		PreviewMaterials.Reset();
		if (Mesh) PreviewMaterials.SetNum(Mesh->Materials.Num());
		SelectedMesh = Mesh;
		PreviewBounds = CalculatePreviewBounds();
	}

	if (PreviewComponent)
	{
		PreviewComponent->SetSkeletalMesh(Mesh);
		PreviewComponent->SetRelativeLocation(FVector::ZeroVector);
		PreviewComponent->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	}
	RebuildStaticPartActors();

	BoneEdit.SelectedBoneIndex = -1;
	BoneEdit.HoveredBoneIndex = -1;
	BoneEdit.BoneGizmo.SetTarget(nullptr, false);
	bFrameRequested = Mesh != nullptr;
	Camera.FocusOnMesh();

	// 메시가 바뀌면 AnimInstance가 새로 만들어지므로 클립과 선택을 다시 맞춘다.
	Animation.BuildPreviewClips();
	Animation.SelectPreviewClip(-1);
}

FBox FSkeletalPreviewTabBase::CalculatePreviewBounds() const
{
	FBox Bounds{FVector(FLT_MAX, FLT_MAX, FLT_MAX), FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX)};
	if (!SelectedMesh) return Bounds;
	for (const FSkeletalMesh& Part : SelectedMesh->MeshData.Meshes)
	{
		for (const FSkeletalVertex& Vertex : Part.Vertices)
		{
			const FVector Position = Part.MeshTransform.TransformPosition(Vertex.Vertex.Position);
			Bounds.Expand(FBox{Position, Position});
		}
	}
	return Bounds;
}


UMaterial* FSkeletalPreviewTabBase::GetPreviewMaterial(int32 SlotIndex) const
{
	if (PreviewMaterials.IsValidIndex(SlotIndex) && PreviewMaterials[SlotIndex])
		return PreviewMaterials[SlotIndex].get();
	return SelectedMesh && SelectedMesh->Materials.IsValidIndex(SlotIndex)
		? SelectedMesh->GetMaterial(SlotIndex) : nullptr;
}

void FSkeletalPreviewTabBase::SetPreviewSampler(int32 SlotIndex, ESamplerState Sampler)
{
	if (!SelectedMesh || !PreviewMaterials.IsValidIndex(SlotIndex)) return;
	UMaterial* Source = SelectedMesh->GetMaterial(SlotIndex);
	if (!Source) return;
	auto& Material = PreviewMaterials[SlotIndex];
	if (Source->SamplerState == Sampler)
	{
		Material.reset();
		return;
	}
	if (!Material)
	{
		Material.reset(UMaterial::CreateInstance(Source));
		if (!Material || (Source->ParamBuffer && !Material->ParamBuffer))
		{
			Material.reset();
			return;
		}
		Material->SetFlags(EObjectFlags::RF_Transient);
	}
	Material->SamplerState = Sampler;
}







void FSkeletalPreviewTabBase::ResetLighting()
{
	bEnableDirectionalLight = true;
	LightYaw = 45.0f;
	LightPitch = 45.0f;
	LightColor = FVector(1.0f, 1.0f, 1.0f);
	LightIntensity = 1.8f;

	bEnableSkyLight = true;
	SkyColor = FVector(0.6f, 0.7f, 0.85f);
	GroundColor = FVector(0.25f, 0.25f, 0.28f);
	SkyLightIntensity = 1.0f;

	BackgroundClearColor = FVector4(0.12f, 0.12f, 0.12f, 1.0f);
	bShowGrid = true;
	bWireframe = false;
	DisplayMode = EViewerDisplayMode::LitBlinnPhong;
}

EViewMode FSkeletalPreviewTabBase::GetRendererViewMode() const
{
	switch (DisplayMode)
	{
	case EViewerDisplayMode::LitLambert:        return EViewMode::Lit_Lambert;
	case EViewerDisplayMode::Unlit:             return EViewMode::Unlit;
	case EViewerDisplayMode::WorldNormal:       return EViewMode::WorldNormal;
	// 히트맵은 PS 앞부분에서 바로 반환하므로 조명 계산이 가장 가벼운 Unlit 변형을 쓴다.
	case EViewerDisplayMode::BoneWeightHeatmap: return EViewMode::Unlit;
	case EViewerDisplayMode::LitBlinnPhong:
	default:                                    return EViewMode::Lit_BlinnPhong;
	}
}



void FSkeletalPreviewTabBase::Tick(float DeltaTime)
{
	// 지운 액터 정리와 새 액터의 BeginPlay. 애니메이션은 아래에서 컴포넌트를 직접 Tick한다.
	if (bPreviewWorldNeedsTick && PreviewScene)
	{
		PreviewScene->Tick(0.0f);
		bPreviewWorldNeedsTick = false;
	}

	if (SaveStatusTimer > 0.0f)
	{
		SaveStatusTimer -= DeltaTime;
		if (SaveStatusTimer <= 0.0f)
		{
			SaveStatusMessage.clear();
		}
	}

	if (Animation.PreviewMode == FSkeletalAnimationPreview::EPreviewMode::BlendSpace)
	{
		if (!Animation.IsBlendSpacePlayable())
		{
			Animation.BlendSpaceMessage = "BlendSpace is empty or incompatible. Select a valid asset to preview.";
			Animation.ActivatePreviewMode(FSkeletalAnimationPreview::EPreviewMode::None);
			Animation.BlendSpacePlayback.Playing = false;
		}
		else if (auto* Instance = Animation.GetPreviewAnimInstance())
		{
			const auto& Axis = *Animation.SelectedBlendSpace->GetBlendParameter(0);
			Animation.BlendSpaceInput = std::clamp(Animation.BlendSpaceInput, Axis.Min, Axis.Max);
			Instance->SetBlendSpaceInput(Animation.BlendSpaceInput);
		}
	}
	if (PreviewComponent && Animation.HasActivePreview() && bViewportOpen && bViewportVisible)
	{
		PreviewComponent->TickComponent(std::min(DeltaTime, 0.1f));
		Animation.UpdatePreviewPlaybackEnd();
	}

	if (!bEmbeddedAnimationPreview) Animation.HandlePreviewShortcuts();
	Camera.HandleInput(DeltaTime);
	Camera.UpdateCamera(DeltaTime);
	if (!bEmbeddedAnimationPreview) BoneEdit.UpdateBoneInteraction();
}













































































std::unique_ptr<FSkeletalPreviewTabBase> FSkeletalPreviewTabBase::CreateAnimationPreviewBackend() const
{
    // 화면(EditorUI)에 붙이지 않는 탭 객체다. 소유자(AnimationPreviewSession)가 Tick·CollectViews·DrawEmbeddedPreview를 직접 부른다.
    FEditorTabSettings Settings;
    Settings.Title = "Animation Preview";
    Settings.Id = "EmbeddedAnimationPreview";
    auto Backend=std::make_unique<FSkeletalPreviewTabBase>(Settings);
    Backend->Init(); Backend->bEmbeddedAnimationPreview=true;
    return Backend;
}
void FSkeletalPreviewTabBase::DrawEmbeddedPreview()
{
    bViewportVisible=false;
    if(ImGui::Button("Reset Camera")) Camera.FocusOnMesh();
    ImGui::SameLine(); ImGui::TextDisabled("Alt+LMB Orbit | Wheel Zoom");
    const auto Size=ImGui::GetContentRegionAvail();
    if(Size.x<2 || Size.y<2) { HideEmbeddedPreview(); return; }
    ResizeViewport((uint32)Size.x,(uint32)Size.y);
    if(bFrameRequested) { Camera.FocusOnMesh(); bFrameRequested=false; }
    FTexture2D* ColorTarget = PreviewViewport.GetColorTarget();
    if(ColorTarget && ColorTarget->GetSRV())
    {
        CachedViewportOrigin=ImGui::GetCursorScreenPos(); CachedViewportSize=Size;
        ImGui::Image(ColorTarget->GetSRV(),Size);
        bViewportVisible=true; bViewportOpen=true; bViewportHovered=ImGui::IsItemHovered();
        bViewportFocused=ImGui::IsWindowFocused();
    }
}


void FSkeletalPreviewTabBase::BeginFrame()
{
	// 뷰포트 패널이 이번 프레임에 다시 그려질 때만 다시 켜진다. 가려진 탭은 렌더·입력을 받지 않는다.
	bViewportVisible = false;
	bViewportHovered = false;
	bAnimationWindowFocused = false;
	bBlendSpaceWindowFocused = false;
	bViewportFocused = false;
}

// ---------------------------------------------------------------------------------------------
// 프리뷰 월드와 View (Week11처럼 그리기는 엔진이 하고, 탭은 월드와 View 옵션만 넘긴다)
// ---------------------------------------------------------------------------------------------

void FSkeletalPreviewTabBase::ResizeViewport(uint32 NewWidth, uint32 NewHeight)
{
	NewWidth = std::max(1u, NewWidth);
	NewHeight = std::max(1u, NewHeight);
	// 도킹 경계를 끄는 동안 1~2픽셀씩 흔들릴 때마다 타깃을 다시 만들지 않는다.
	if (PreviewViewport.IsActive() &&
		std::abs(static_cast<int32>(ViewportWidth) - static_cast<int32>(NewWidth)) <= 2 &&
		std::abs(static_cast<int32>(ViewportHeight) - static_cast<int32>(NewHeight)) <= 2)
	{
		return;
	}

	ViewportWidth = NewWidth;
	ViewportHeight = NewHeight;
	PreviewViewport.SetRect({ 0.0f, 0.0f, static_cast<float>(ViewportWidth), static_cast<float>(ViewportHeight) }, true);
}

void FSkeletalPreviewTabBase::RebuildStaticPartActors()
{
	if (!PreviewScene)
		return;

	for (AStaticMeshActor* Actor : StaticPartActors)
		PreviewScene->DestroyActor(Actor);
	StaticPartActors.Reset();
	bPreviewWorldNeedsTick = true;

	VisibleStaticMeshes.Reset();
	if (!SelectedMesh)
		return;
	const FFbxAssetSet* Assets = UAssetManager::Get().FindFbxAssets(SelectedMesh->GetPath());
	if (!Assets)
		return;

	// FBX 노드의 원래 배치에 둔다. 처음에는 숨겨 두고 Asset Details의 체크박스(VisibleStaticMeshes)로 켠다.
	for (const FFbxStaticAsset& Part : Assets->StaticMeshes)
	{
		if (!Part.Mesh)
			continue;
		AStaticMeshActor* Actor = PreviewScene->SpawnActor<AStaticMeshActor>();
		if (!Actor)
			continue;
		UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		Component->SetStaticMesh(Part.Mesh);
		Component->SetWorldTransform(Part.NodeTransform, false);
		Component->SetVisible(false);
		StaticPartActors.Add(Actor);
	}
}

void FSkeletalPreviewTabBase::SyncPreviewWorld()
{
	// 1. 조명: 프리뷰 설정 패널 값을 월드의 라이트 액터에 옮긴다.
	const auto ToColor = [](const FVector& Color)
	{
		const auto Channel = [](float Value) { return static_cast<uint8>(std::clamp(Value, 0.0f, 1.0f) * 255.0f + 0.5f); };
		return FColor(Channel(Color.X), Channel(Color.Y), Channel(Color.Z), 255);
	};
	if (DirectionalLightActor)
	{
		ULightComponent* Light = DirectionalLightActor->GetLightComponent();
		Light->SetRelativeRotation(FRotator(LightPitch, LightYaw, 0.0f));
		Light->SetLightColor(ToColor(LightColor));
		Light->SetIntensity(bEnableDirectionalLight ? LightIntensity : 0.0f);
	}
	if (SkyLightActor)
	{
		ULightComponent* Sky = SkyLightActor->GetLightComponent();
		Sky->SetLightColor(ToColor(SkyColor));
		Sky->SetIntensity(SkyLightIntensity);
		Sky->SetVisibility(bEnableSkyLight);
	}

	// 2. 메시 머티리얼(샘플러 덮어쓰기)과 FBX 정적 파츠 표시
	if (PreviewComponent && SelectedMesh)
	{
		for (int32 Slot = 0; Slot < SelectedMesh->Materials.Num(); ++Slot)
			PreviewComponent->SetMaterial(Slot, GetPreviewMaterial(Slot));
	}
	for (AStaticMeshActor* Actor : StaticPartActors)
	{
		UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		Component->SetVisible(VisibleStaticMeshes.Find(Component->GetStaticMesh()) != INDEX_NONE);
	}

	// 3. 카메라와 표시 모드: 엔진은 이 Client로 그린다. 피킹과 같은 카메라(UpdateCamera 결과)를 쓴다.
	FEditorViewportClient& Client = PreviewViewport.GetClient();
	FCameraProjection Projection{};
	Projection.Mode = EProjectionMode::Perspective;
	Projection.FovDegrees = Camera.FovDegrees;
	Projection.OrthoWidth = 0.0f;
	Projection.NearClip = std::max(MeshRadius * 0.001f, 0.00001f);
	Projection.FarClip = std::max(Camera.CameraDistance, (Camera.TargetPosition - Camera.CameraLocation).Length()) + MeshRadius * 100.0f;
	const FQuat Rotation(Camera.CachedCameraRotation);
	Client.SetCameraOverride(Camera.CachedCameraLocation, Rotation.GetForwardVector(), Projection, PreviewViewport.GetRect(), &Rotation);
	Client.SetViewMode(GetRendererViewMode());
	Client.SetWireframe(bWireframe);

	// 4. 본 Gizmo 대상: 선택한 본의 현재 월드 위치·회전으로 옮긴다. (드래그 중에는 Gizmo가 직접 움직인다)
	const bool bShowGizmo = BoneEdit.bShowBoneGizmo && !Animation.HasActivePreview() && BoneEdit.BoneGizmoTarget && SelectedMesh
		&& BoneEdit.SelectedBoneIndex >= 0 && BoneEdit.SelectedBoneIndex < static_cast<int32>(SelectedMesh->MeshData.Bones.Num());
	if (!bShowGizmo)
	{
		BoneEdit.BoneGizmo.SetTarget(nullptr, false);
	}
	else if (!BoneEdit.BoneGizmo.IsUsing())
	{
		TArray<FMatrix> BoneMatrices;
		BoneEdit.ComputeBoneWorldMatrices(BoneMatrices);
		if (BoneEdit.SelectedBoneIndex < BoneMatrices.Num())
		{
			const FMatrix& BoneMat = BoneMatrices[BoneEdit.SelectedBoneIndex];
			FMatrix BoneRotMat = FMatrix::Identity;
			BoneRotMat.SetAxis(0, FVector(BoneMat.M[0][0], BoneMat.M[0][1], BoneMat.M[0][2]).GetSafeNormal());
			BoneRotMat.SetAxis(1, FVector(BoneMat.M[1][0], BoneMat.M[1][1], BoneMat.M[1][2]).GetSafeNormal());
			BoneRotMat.SetAxis(2, FVector(BoneMat.M[2][0], BoneMat.M[2][1], BoneMat.M[2][2]).GetSafeNormal());
			BoneEdit.BoneGizmoTarget->SetRelativeLocation(FVector(BoneMat.M[3][0], BoneMat.M[3][1], BoneMat.M[3][2]));
			BoneEdit.BoneGizmoTarget->SetRelativeRotation(MatrixToRotator(BoneRotMat));
			BoneEdit.BoneGizmoTarget->SetRelativeScale3D(BoneEdit.GetBoneLocalTransform(BoneEdit.SelectedBoneIndex).Scale);
			BoneEdit.BoneGizmo.SetTarget(BoneEdit.BoneGizmoTarget, false);
		}
	}
}

void FSkeletalPreviewTabBase::CollectViews(TArray<FEditorTabView>& OutViews)
{
	if (!bViewportVisible || !SelectedMesh || !PreviewScene || !PreviewScene->GetWorld() || !PreviewViewport.IsActive())
		return;
	const FRenderingInfo* RenderingInfo = PreviewViewport.GetRenderingInfo();
	if (!RenderingInfo || RenderingInfo->ColorRenderTargets.IsEmpty())
		return;

	Camera.UpdateCamera(0.0f);
	SyncPreviewWorld();

	// 배경은 스카이박스 대신 지우기 색이다. (타깃을 공유하지 않으므로 매 프레임 덮어써도 된다)
	const_cast<FRenderingInfo*>(RenderingInfo)->ColorRenderTargets[0].ClearValue.ColorClearValue = BackgroundClearColor;

	FEditorTabView TabView;
	TabView.World = PreviewScene->GetWorld();
	TabView.RenderingInfo = RenderingInfo;
	FEditorSceneView& View = TabView.View;
	View.Client = &PreviewViewport.GetClient();
	View.Rect = PreviewViewport.GetRect();
	View.HDRTarget = PreviewViewport.GetHDRTarget();
	View.FxaaTarget = PreviewViewport.GetFxaaTarget();
	View.ShadowViewId = PreviewViewport.GetShadowViewId();
	// 레벨용 아이콘·Outline·라이트 화살표는 그리지 않는다. 그리드만 따로 켠다.
	View.bShowEditorOverlay = false;
	View.bDrawSkybox = false;
	View.GridMode = bShowGrid ? EViewGridMode::Show : EViewGridMode::Hide;
	View.ExposureOverride = PreviewExposure;

	if (IsBoneWeightHeatmapMode())
	{
		View.BoneWeightHeatmapStatus = BoneWeightHeatmap.Update(SelectedMesh, BoneEdit.SelectedBoneIndex)
			? FBoneWeightHeatmap::StatusOn : FBoneWeightHeatmap::StatusNoSelection;
		View.BoneWeightHeatmap = &BoneWeightHeatmap;
	}

	if (BoneEdit.bShowBones && PreviewComponent)
	{
		BoneEdit.SkeletonDraw.Bones = &SelectedMesh->MeshData.Bones;
		BoneEdit.ComputeBoneWorldMatrices(BoneEdit.SkeletonDraw.BoneWorldMatrices);
		BoneEdit.SkeletonDraw.SelectedBone = BoneEdit.SelectedBoneIndex;
		BoneEdit.SkeletonDraw.HoveredBone = BoneEdit.HoveredBoneIndex;
		BoneEdit.SkeletonDraw.bDrawCones = BoneEdit.bDrawBoneCones;
		BoneEdit.SkeletonDraw.ConeWidthRatio = BoneEdit.BoneConeWidthRatio;
		BoneEdit.SkeletonDraw.JointRadius = BoneEdit.BoneJointRadius;
		BoneEdit.SkeletonDraw.MeshRadius = MeshRadius;
		View.Skeleton = &BoneEdit.SkeletonDraw;
	}
	if (BoneEdit.BoneGizmo.GetTarget())
		View.PreviewGizmo = &BoneEdit.BoneGizmo;

	OutViews.Add(TabView);
}

#include "EnginePCH.h"
#include "Editor/SkeletalMeshViewer/SkeletalPreviewCamera.h"
#include "Editor/SkeletalMeshViewer/SkeletalPreviewTabBase.h"
#include "Editor/SkeletalMeshViewer/SkeletalMeshEditorUtils.h"
#include "Animation/AssetAnimInstance.h"
#include "Animation/AnimGraph.h"
#include "SkeletalMeshEditorUtils.h"
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

using namespace SkeletalMeshEditorUtils;

FSkeletalPreviewCamera::FSkeletalPreviewCamera(FSkeletalPreviewTabBase& InTab)
	: Tab(InTab)
{
}

// 셰이크 패턴(PreviewShake)의 완전한 타입이 이 파일에 있어야 unique_ptr가 지울 수 있다.
FSkeletalPreviewCamera::~FSkeletalPreviewCamera() = default;

void FSkeletalPreviewCamera::FocusOnMesh()
{
	if (!Tab.SelectedMesh)
	{
		TargetPosition = FVector::ZeroVector;
		CameraDistance = 250.0f;
		CameraSpeed = 500.0f;
		CameraYaw = 45.0f;
		CameraPitch = 20.0f;
		const FRotator Rot(CameraPitch, CameraYaw, 0.0f);
		CameraLocation = TargetPosition - Rot.Quaternion().GetForwardVector() * CameraDistance;
		return;
	}

	FBox Bounds = Tab.PreviewBounds;
	if (!Tab.VisibleStaticMeshes.IsEmpty())
	{
		if (const FFbxAssetSet* Assets = UAssetManager::Get().FindFbxAssets(Tab.SelectedMesh->GetPath()))
		{
			for (const FFbxStaticAsset& Part : Assets->StaticMeshes)
			{
				if (Part.Mesh && Tab.VisibleStaticMeshes.Find(Part.Mesh) != INDEX_NONE)
					Bounds.Expand(Part.Mesh->GetRenderBounds().GetWorldAABB(Part.NodeTransform));
			}
		}
	}
	if (Bounds.Min.X <= Bounds.Max.X)
	{
		TargetPosition = (Bounds.Min + Bounds.Max) * 0.5f;
		const FVector Extent = (Bounds.Max - Bounds.Min) * 0.5f;
		Tab.MeshRadius = std::max(Extent.Length(), 0.001f);
		const float Aspect = Tab.ViewportHeight > 0 ? static_cast<float>(Tab.ViewportWidth) / Tab.ViewportHeight : 1.0f;
		const float HalfFov = FMath::DegreesToRadians(FovDegrees) * 0.5f;
		const float HalfHorizontalFov = std::atan(std::tan(HalfFov) * Aspect);
		CameraDistance = Tab.MeshRadius / std::sin(std::min(HalfFov, HalfHorizontalFov)) * 1.1f;
		CameraSpeed = Tab.MeshRadius * 2.5f;
	}
	else
	{
		TargetPosition = FVector::ZeroVector;
		CameraDistance = 250.0f;
		CameraSpeed = 500.0f;
	}

	const FRotator Rot(CameraPitch, CameraYaw, 0.0f);
	CameraLocation = TargetPosition - Rot.Quaternion().GetForwardVector() * CameraDistance;
}

void FSkeletalPreviewCamera::HandleInput(float DeltaTime)
{
	ImGuiIO& IO = ImGui::GetIO();

	const bool bRightMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
	const bool bMiddleMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
	const bool bLeftMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);

	if (Tab.bViewportHovered && (bRightMouseDown || bMiddleMouseDown || bLeftMouseDown))
	{
		Tab.bIsInteracting = true;
	}
	else if (!bRightMouseDown && !bMiddleMouseDown && !bLeftMouseDown)
	{
		Tab.bIsInteracting = false;
	}

	if (!Tab.bViewportHovered && !Tab.bIsInteracting)
		return;

	// 1. 단축키 'F': 포커스 맞추기, W/E/R/Space: 기즈모 모드 전환, Ctrl+S: 저장 (우클릭 카메라 조작 중이 아닐 때)
	if (!IO.WantTextInput && !bRightMouseDown && !IO.KeyAlt)
	{
		if (!Tab.bEmbeddedAnimationPreview && IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
		{
			Tab.AssetSaver.SaveCurrentMesh();
		}
		if (ImGui::IsKeyPressed(ImGuiKey_F))
		{
			FocusOnMesh();
		}
		if (ImGui::IsKeyPressed(ImGuiKey_W))
		{
			Tab.BoneEdit.BoneGizmo.SetMode(EGizmoMode::Location);
		}
		if (ImGui::IsKeyPressed(ImGuiKey_E))
		{
			Tab.BoneEdit.BoneGizmo.SetMode(EGizmoMode::Rotation);
		}
		if (ImGui::IsKeyPressed(ImGuiKey_R))
		{
			Tab.BoneEdit.BoneGizmo.SetMode(EGizmoMode::Scale);
		}
		if (!Tab.Animation.HasActivePreview() && ImGui::IsKeyPressed(ImGuiKey_Space))
		{
			const EGizmoMode Cur = Tab.BoneEdit.BoneGizmo.GetMode();
			if (Cur == EGizmoMode::Location) Tab.BoneEdit.BoneGizmo.SetMode(EGizmoMode::Rotation);
			else if (Cur == EGizmoMode::Rotation) Tab.BoneEdit.BoneGizmo.SetMode(EGizmoMode::Scale);
			else Tab.BoneEdit.BoneGizmo.SetMode(EGizmoMode::Location);
		}
	}

	// 2. 단축키 'L' + 좌클릭 드래그: 실시간 라이트 회전
	const bool bHoldingL = ImGui::IsKeyDown(ImGuiKey_L);
	if (bHoldingL && bLeftMouseDown)
	{
		Tab.LightYaw += IO.MouseDelta.x * 0.5f;
		Tab.LightPitch = std::clamp(Tab.LightPitch + IO.MouseDelta.y * 0.5f, -89.0f, 89.0f);
		return;
	}

	// 3. 우클릭으로 1인칭 카메라를 이동하고 시점을 회전한다.
	if (bRightMouseDown && !IO.KeyAlt)
	{
		CameraYaw += IO.MouseDelta.x * 0.35f;
		CameraPitch = std::clamp(CameraPitch + IO.MouseDelta.y * 0.35f, -89.0f, 89.0f);

		const FRotator CurrentRot(CameraPitch, CameraYaw, 0.0f);
		const FQuat CurrentQuat(CurrentRot);
		const FVector CurrentForward = CurrentQuat.GetForwardVector();
		const FVector CurrentRight = CurrentQuat.GetRightVector();
		const FVector CurrentUp = FVector(0.0f, 0.0f, 1.0f);

		FVector MoveDir = FVector::ZeroVector;
		if (ImGui::IsKeyDown(ImGuiKey_W)) MoveDir += CurrentForward;
		if (ImGui::IsKeyDown(ImGuiKey_S)) MoveDir -= CurrentForward;
		if (ImGui::IsKeyDown(ImGuiKey_D)) MoveDir += CurrentRight;
		if (ImGui::IsKeyDown(ImGuiKey_A)) MoveDir -= CurrentRight;
		if (ImGui::IsKeyDown(ImGuiKey_E)) MoveDir += CurrentUp;
		if (ImGui::IsKeyDown(ImGuiKey_Q)) MoveDir -= CurrentUp;

		if (MoveDir.SizeSquared() > 0.0001f)
		{
			const float SpeedMultiplier = IO.KeyShift ? 2.5f : 1.0f;
			const FVector DeltaPos = MoveDir.GetSafeNormal() * (CameraSpeed * SpeedMultiplier * DeltaTime);
			CameraLocation += DeltaPos;
		}

		TargetPosition = CameraLocation + CurrentForward * CameraDistance;
	}
	// 4. Alt + 마우스 조작
	else if (IO.KeyAlt)
	{
		if (bLeftMouseDown)
		{
			CameraYaw += IO.MouseDelta.x * 0.35f;
			CameraPitch = std::remainder(CameraPitch + IO.MouseDelta.y * 0.35f, 360.0f);

			const FRotator CurrentRot(CameraPitch, CameraYaw, 0.0f);
			const FQuat CurrentQuat(CurrentRot);
			CameraLocation = TargetPosition - CurrentQuat.GetForwardVector() * CameraDistance;
		}
		else if (bRightMouseDown)
		{
			const float ZoomDelta = (IO.MouseDelta.x - IO.MouseDelta.y) * std::max(CameraDistance * 0.005f, Tab.MeshRadius * 0.0001f);
			CameraDistance = std::clamp(CameraDistance - ZoomDelta, Tab.MeshRadius * 0.05f, Tab.MeshRadius * 1000.0f);

			const FRotator CurrentRot(CameraPitch, CameraYaw, 0.0f);
			const FQuat CurrentQuat(CurrentRot);
			CameraLocation = TargetPosition - CurrentQuat.GetForwardVector() * CameraDistance;
		}
	}

	// 5. 휠 클릭 드래그(미들 버튼): 카메라 팬
	if (bMiddleMouseDown)
	{
		const FRotator CurrentRot(CameraPitch, CameraYaw, 0.0f);
		const FQuat CurrentQuat(CurrentRot);
		const FVector CurrentRight = CurrentQuat.GetRightVector();
		const FVector CurrentUp = FVector(0.0f, 0.0f, 1.0f);

		const float PanSpeed = std::max(CameraDistance * 0.0015f, 0.005f);
		const FVector PanDelta = (-CurrentRight * (IO.MouseDelta.x * PanSpeed)) + (-CurrentUp * (IO.MouseDelta.y * PanSpeed));
		CameraLocation += PanDelta;
		TargetPosition += PanDelta;
	}

	// 6. 마우스 휠로 카메라를 앞뒤로 이동해 확대하거나 축소한다.
	if (std::abs(IO.MouseWheel) > 0.01f)
	{
		const float Step = std::max(CameraDistance * 0.15f, Tab.MeshRadius * 0.005f);
		CameraDistance -= IO.MouseWheel * Step;

		const FRotator CurrentRot(CameraPitch, CameraYaw, 0.0f);
		const FQuat CurrentQuat(CurrentRot);
		const FVector CurrentForward = CurrentQuat.GetForwardVector();

		CameraDistance = std::clamp(CameraDistance, Tab.MeshRadius * 0.05f, Tab.MeshRadius * 1000.0f);
		CameraLocation = TargetPosition - CurrentForward * CameraDistance;
	}
}

void FSkeletalPreviewCamera::UpdateCamera(float DeltaTime)
{
	CachedCameraLocation = CameraLocation;

	FMinimalViewInfo ViewInfo;
	ViewInfo.SetLocation(CachedCameraLocation);
	ViewInfo.SetRotation(FRotator(CameraPitch, CameraYaw, 0.0f));
	ViewInfo.SetFieldOfView(FovDegrees);
	ViewInfo.SetAspectRatio((Tab.ViewportHeight > 0) ? (static_cast<float>(Tab.ViewportWidth) / static_cast<float>(Tab.ViewportHeight)) : (16.0f / 9.0f));
	ViewInfo.SetNearZ(std::max(Tab.MeshRadius * 0.001f, 0.00001f));
	ViewInfo.SetFarZ(std::max(CameraDistance, (TargetPosition - CameraLocation).Length()) + Tab.MeshRadius * 100.0f);

	if (PreviewShake)
	{
		FCameraShakeUpdateParams Params;
		Params.DeltaTime = DeltaTime;
		Params.POV = ViewInfo;
		FCameraShakeUpdateResult Result;
		PreviewShake->UpdateShakePattern(Params, Result);
		UCameraModifier_CameraShake::ApplyResult(Result, PreviewShakeScale, ECameraShakePlaySpace::CameraLocal, FRotator{}, ViewInfo);
		if (PreviewShake->IsFinished())
		{
			PreviewShake.reset();
		}
	}

	// 셰이크까지 반영한 최종 카메라. 렌더(View)와 피킹이 같은 값을 쓴다.
	CachedCameraLocation = ViewInfo.GetLocation();
	CachedCameraRotation = ViewInfo.GetRotation();
	CachedViewMatrix = ViewInfo.GetViewMatrix();
	CachedProjMatrix = ViewInfo.GetPerspectiveMatrix();
}


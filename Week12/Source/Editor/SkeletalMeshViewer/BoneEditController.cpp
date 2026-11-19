#include "EnginePCH.h"
#include "Editor/SkeletalMeshViewer/BoneEditController.h"
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

FTransform FBoneEditController::GetBoneLocalTransform(int32 BoneIndex) const
{
	if (Tab.PreviewComponent)
	{
		FTransform Transform;
		if (Tab.PreviewComponent->GetBoneLocalTransform(BoneIndex, Transform))
			return Transform;
	}
	return FTransform{};
}

void FBoneEditController::ResetAllBonesToBindPose()
{
	if (Tab.Animation.BlockEditDuringAnimationPreview())
	{
		return;
	}

	if (Tab.PreviewComponent)
	{
		Tab.PreviewComponent->ResetPoseToBindPose();
		TArray<FMatrix> BoneMatrices;
		ComputeBoneWorldMatrices(BoneMatrices);
		if (SelectedBoneIndex >= 0 && SelectedBoneIndex < BoneMatrices.Num() && BoneGizmoTarget)
		{
			const FMatrix& BoneMat = BoneMatrices[SelectedBoneIndex];
			const FVector BoneWorldPos(BoneMat.M[3][0], BoneMat.M[3][1], BoneMat.M[3][2]);
			FMatrix BoneRotMat = FMatrix::Identity;
			BoneRotMat.SetAxis(0, FVector(BoneMat.M[0][0], BoneMat.M[0][1], BoneMat.M[0][2]).GetSafeNormal());
			BoneRotMat.SetAxis(1, FVector(BoneMat.M[1][0], BoneMat.M[1][1], BoneMat.M[1][2]).GetSafeNormal());
			BoneRotMat.SetAxis(2, FVector(BoneMat.M[2][0], BoneMat.M[2][1], BoneMat.M[2][2]).GetSafeNormal());

			BoneGizmoTarget->SetRelativeLocation(BoneWorldPos);
			BoneGizmoTarget->SetRelativeRotation(MatrixToRotator(BoneRotMat));
			BoneGizmoTarget->SetRelativeScale3D(GetBoneLocalTransform(SelectedBoneIndex).Scale);
		}
		Tab.SaveStatusMessage = "Reset all bones to bind pose.";
		Tab.SaveStatusTimer = 2.5f;
	}
}

void FBoneEditController::ComputeBoneWorldMatrices(TArray<FMatrix>& OutBoneWorldMatrices) const
{
	OutBoneWorldMatrices.Empty();
	if (!Tab.SelectedMesh)
		return;

	const TArray<FSkeletalBone>& Bones = Tab.SelectedMesh->MeshData.Bones;
	const int32 NumBones = Bones.Num();
	if (NumBones == 0)
		return;

	OutBoneWorldMatrices.SetNum(NumBones);
	TArray<uint8> State;
	State.SetNum(NumBones);
	for (int32 i = 0; i < NumBones; ++i)
	{
		State[i] = 0;
	}

	const FMatrix CompWorldMatrix = Tab.PreviewComponent ? Tab.PreviewComponent->GetWorldMatrix() : FMatrix::Identity;

	std::function<void(int32)> EvaluateBone = [&](int32 Idx)
	{
		if (Idx < 0 || Idx >= NumBones)
			return;
		if (State[Idx] == 2)
			return;
		if (State[Idx] == 1) // 순환 참조 방지
		{
			OutBoneWorldMatrices[Idx] = FMatrix::Identity;
			State[Idx] = 2;
			return;
		}

		State[Idx] = 1;

		FTransform LocalTf;
		if (!Tab.PreviewComponent || !Tab.PreviewComponent->GetBoneLocalTransform(Idx, LocalTf))
			LocalTf = Bones[Idx].RefPoseTransform;
		const FMatrix LocalMat = LocalTf.GetLocalMatrix();

		const int32 ParentIdx = Bones[Idx].ParentIndex;
		if (ParentIdx >= 0 && ParentIdx < NumBones && ParentIdx != Idx)
		{
			EvaluateBone(ParentIdx);
			OutBoneWorldMatrices[Idx] = LocalMat * OutBoneWorldMatrices[ParentIdx];
		}
		else
		{
			OutBoneWorldMatrices[Idx] = LocalMat * CompWorldMatrix;
		}

		State[Idx] = 2;
	};

	for (int32 i = 0; i < NumBones; ++i)
	{
		EvaluateBone(i);
	}
}

bool FBoneEditController::IsBoneAncestorOf(int32 AncestorIndex, int32 TargetBoneIndex, const TArray<FSkeletalBone>& Bones) const
{
	if (AncestorIndex < 0 || TargetBoneIndex < 0 || AncestorIndex >= Bones.Num() || TargetBoneIndex >= Bones.Num())
		return false;

	int32 Cur = Bones[TargetBoneIndex].ParentIndex;
	int32 Safety = 0;
	while (Cur >= 0 && Cur < Bones.Num() && Safety < Bones.Num())
	{
		if (Cur == AncestorIndex)
			return true;
		Cur = Bones[Cur].ParentIndex;
		++Safety;
	}
	return false;
}

void FBoneEditController::UpdateBoneInteraction()
{
	if (!Tab.SelectedMesh || Tab.CachedViewportSize.x <= 1.0f || Tab.CachedViewportSize.y <= 1.0f)
		return;

	const TArray<FSkeletalBone>& Bones = Tab.SelectedMesh->MeshData.Bones;
	const int32 NumBones = Bones.Num();
	if (NumBones == 0)
		return;

	TArray<FMatrix> BoneMatrices;
	ComputeBoneWorldMatrices(BoneMatrices);
	if (BoneMatrices.Num() != NumBones)
		return;

	const FMatrix ViewProj = Tab.Camera.CachedViewMatrix * Tab.Camera.CachedProjMatrix;
	ImGuiIO& IO = ImGui::GetIO();
	const bool bCameraManipulating = ImGui::IsMouseDown(ImGuiMouseButton_Right)
		|| ImGui::IsMouseDown(ImGuiMouseButton_Middle)
		|| IO.KeyAlt
		|| ImGui::IsKeyDown(ImGuiKey_L);

	bool bGizmoBlocksPick = false;
	if (bShowBoneGizmo && !Tab.Animation.HasActivePreview() && Tab.PreviewComponent && BoneGizmoTarget && SelectedBoneIndex >= 0 && SelectedBoneIndex < NumBones)
	{
		if (!BoneGizmo.IsUsing())
		{
			const FMatrix& BoneMat = BoneMatrices[SelectedBoneIndex];
			const FVector BoneWorldPos(BoneMat.M[3][0], BoneMat.M[3][1], BoneMat.M[3][2]);
			FMatrix BoneRotMat = FMatrix::Identity;
			BoneRotMat.SetAxis(0, FVector(BoneMat.M[0][0], BoneMat.M[0][1], BoneMat.M[0][2]).GetSafeNormal());
			BoneRotMat.SetAxis(1, FVector(BoneMat.M[1][0], BoneMat.M[1][1], BoneMat.M[1][2]).GetSafeNormal());
			BoneRotMat.SetAxis(2, FVector(BoneMat.M[2][0], BoneMat.M[2][1], BoneMat.M[2][2]).GetSafeNormal());

			BoneGizmoTarget->SetRelativeLocation(BoneWorldPos);
			BoneGizmoTarget->SetRelativeRotation(MatrixToRotator(BoneRotMat));
			BoneGizmoTarget->SetRelativeScale3D(GetBoneLocalTransform(SelectedBoneIndex).Scale);
			BoneGizmo.SetTarget(BoneGizmoTarget, false);
		}

		if ((Tab.bViewportHovered && !bCameraManipulating) || BoneGizmo.IsUsing())
		{
			const FVector2 LocalMousePos(IO.MousePos.x - Tab.CachedViewportOrigin.x, IO.MousePos.y - Tab.CachedViewportOrigin.y);
			const float NdcX = (LocalMousePos.X / Tab.CachedViewportSize.x) * 2.0f - 1.0f;
			const float NdcY = 1.0f - (LocalMousePos.Y / Tab.CachedViewportSize.y) * 2.0f;

			const FMatrix InvViewProj = ViewProj.Inverse();
			FVector4 NearClip = InvViewProj.TransformFVector4(FVector4(NdcX, NdcY, 0.0f, 1.0f));
			FVector4 FarClip = InvViewProj.TransformFVector4(FVector4(NdcX, NdcY, 1.0f, 1.0f));
			if (std::abs(NearClip.W) > 1e-6f) NearClip /= NearClip.W;
			if (std::abs(FarClip.W) > 1e-6f) FarClip /= FarClip.W;

			FRay MouseRay{};
			MouseRay.Origin = Tab.Camera.CachedCameraLocation;
			MouseRay.Direction = (FVector(FarClip.X, FarClip.Y, FarClip.Z) - FVector(NearClip.X, NearClip.Y, NearClip.Z)).GetSafeNormal();

			const bool bLeftMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left) && !bCameraManipulating;
			BoneGizmo.Update(
				MouseRay,
				LocalMousePos,
				ViewProj,
				static_cast<int>(Tab.CachedViewportSize.x),
				static_cast<int>(Tab.CachedViewportSize.y),
				bLeftMouseDown,
				Tab.Camera.CachedCameraLocation,
				false);

			if (BoneGizmo.IsUsing())
			{
				FTransform NewLocalTf = GetBoneLocalTransform(SelectedBoneIndex);
				const int32 ParentIdx = Bones[SelectedBoneIndex].ParentIndex;
				const FMatrix ParentWorldMat = (ParentIdx >= 0 && ParentIdx < NumBones)
					? BoneMatrices[ParentIdx]
					: Tab.PreviewComponent->GetWorldMatrix();

				if (BoneGizmo.GetMode() == EGizmoMode::Rotation)
				{
					const FMatrix NewWorldRotMat = BoneGizmoTarget->GetRelativeRotation().Quaternion().ToFMatrix();
					FMatrix ParentRotMat = FMatrix::Identity;
					ParentRotMat.SetAxis(0, FVector(ParentWorldMat.M[0][0], ParentWorldMat.M[0][1], ParentWorldMat.M[0][2]).GetSafeNormal());
					ParentRotMat.SetAxis(1, FVector(ParentWorldMat.M[1][0], ParentWorldMat.M[1][1], ParentWorldMat.M[1][2]).GetSafeNormal());
					ParentRotMat.SetAxis(2, FVector(ParentWorldMat.M[2][0], ParentWorldMat.M[2][1], ParentWorldMat.M[2][2]).GetSafeNormal());

					const FMatrix NewLocalRotMat = NewWorldRotMat * ParentRotMat.GetTransposed();
					NewLocalTf.Rotation = MatrixToRotator(NewLocalRotMat);
				}
				else if (BoneGizmo.GetMode() == EGizmoMode::Location)
				{
					const FVector NewWorldPos = BoneGizmoTarget->GetWorldLocation();
					NewLocalTf.Location = ParentWorldMat.Inverse().TransformPosition(NewWorldPos);
				}
				else if (BoneGizmo.GetMode() == EGizmoMode::Scale)
				{
					NewLocalTf.Scale = BoneGizmoTarget->GetRelativeScale3D();
				}

				Tab.PreviewComponent->SetBoneLocalTransform(SelectedBoneIndex, NewLocalTf);
				ComputeBoneWorldMatrices(BoneMatrices);
			}
		}

		bGizmoBlocksPick = BoneGizmo.IsUsing() || (BoneGizmo.GetHoveredAxis() >= 0);
	}
	else
	{
		BoneGizmo.SetTarget(nullptr, false);
	}

	auto ProjectPoint = [&](const FVector& WorldPos, ImVec2& OutScreen) -> bool
	{
		const FVector4 Clip = ViewProj.TransformFVector4(FVector4(WorldPos.X, WorldPos.Y, WorldPos.Z, 1.0f));
		if (Clip.W <= 0.1f)
			return false;

		const float InvW = 1.0f / Clip.W;
		const float NdcX = Clip.X * InvW;
		const float NdcY = Clip.Y * InvW;

		OutScreen.x = Tab.CachedViewportOrigin.x + (NdcX * 0.5f + 0.5f) * Tab.CachedViewportSize.x;
		OutScreen.y = Tab.CachedViewportOrigin.y + (1.0f - (NdcY * 0.5f + 0.5f)) * Tab.CachedViewportSize.y;
		return true;
	};

	TArray<ImVec2> BoneScreenPts;
	TArray<uint8> BoneInFront;
	BoneScreenPts.SetNum(NumBones);
	BoneInFront.SetNum(NumBones);
	for (int32 i = 0; i < NumBones; ++i)
	{
		const FMatrix& Mat = BoneMatrices[i];
		const FVector Pos(Mat.M[3][0], Mat.M[3][1], Mat.M[3][2]);
		BoneInFront[i] = ProjectPoint(Pos, BoneScreenPts[i]) ? 1 : 0;
	}

	// 뷰포트 마우스 호버 및 좌클릭 본 피킹 (기즈모 조작 중이 아닐 때만 수행)
	HoveredBoneIndex = -1;
	if (Tab.bViewportHovered && !bCameraManipulating && !bGizmoBlocksPick && bShowBones)
	{
		const FVector2 MousePt(IO.MousePos.x, IO.MousePos.y);
		float BestJointScore = 14.0f;
		int32 BestJointBone = -1;

		for (int32 i = 0; i < NumBones; ++i)
		{
			if (!BoneInFront[i])
				continue;

			const float dx = MousePt.X - BoneScreenPts[i].x;
			const float dy = MousePt.Y - BoneScreenPts[i].y;
			const float Dist = std::sqrt(dx * dx + dy * dy);
			if (Dist < BestJointScore)
			{
				BestJointScore = Dist;
				BestJointBone = i;
			}
		}

		if (BestJointBone >= 0)
		{
			HoveredBoneIndex = BestJointBone;
		}
		else
		{
			float BestSegDist = 9.0f;
			int32 BestSegBone = -1;
			for (int32 i = 0; i < NumBones; ++i)
			{
				const int32 ParentIdx = Bones[i].ParentIndex;
				if (ParentIdx < 0 || ParentIdx >= NumBones)
					continue;
				if (!BoneInFront[ParentIdx] || !BoneInFront[i])
					continue;

				const FVector2 SegA(BoneScreenPts[ParentIdx].x, BoneScreenPts[ParentIdx].y);
				const FVector2 SegB(BoneScreenPts[i].x, BoneScreenPts[i].y);
				const float SegDist = DistanceToSegment(MousePt, SegA, SegB);
				if (SegDist < BestSegDist)
				{
					BestSegDist = SegDist;
					BestSegBone = ParentIdx;
				}
			}
			HoveredBoneIndex = BestSegBone;
		}

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			SelectedBoneIndex = HoveredBoneIndex;
			if (SelectedBoneIndex >= 0)
			{
				bScrollToSelectedBone = true;
			}
		}
	}
}


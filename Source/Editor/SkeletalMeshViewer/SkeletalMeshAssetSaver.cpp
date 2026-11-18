#include "EnginePCH.h"
#include "Editor/SkeletalMeshViewer/SkeletalMeshAssetSaver.h"
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

bool FSkeletalMeshAssetSaver::SaveCurrentMesh()
{
	if (Tab.Animation.BlockEditDuringAnimationPreview())
	{
		return false;
	}

	if (!Tab.SelectedMesh)
	{
		Tab.SaveStatusMessage = "No skeletal mesh selected.";
		Tab.SaveStatusTimer = 3.0f;
		return false;
	}

	// 1. PreviewComponent에서 수정한 본의 현재 LocalTransform들을 SelectedMesh->MeshData.Bones에 반영
	// 주의: Mesh.Vertices는 원본 FBX 바인드 공간 정점이므로 InverseBindPoseMat은 원본을 유지해야
	// ResetPoseToBindPose() -> UpdateSkinning() 시 정점들이 수정된 본 위치를 따라 스키닝된다.
	if (Tab.PreviewComponent)
	{
		const int32 NumBones = Tab.SelectedMesh->MeshData.Bones.Num();
		for (int32 BoneIdx = 0; BoneIdx < NumBones; ++BoneIdx)
		{
			FTransform CurTf;
			if (Tab.PreviewComponent->GetBoneLocalTransform(BoneIdx, CurTf))
			{
				Tab.SelectedMesh->MeshData.Bones[BoneIdx].RefPoseTransform = CurTf;
			}
		}
	}

	// 2. AssetManager를 통해 .bin 파일 저장 및 씬 컴포넌트(레벨 에디터 및 프리뷰) 즉시 갱신
	if (UAssetManager::Get().SaveSkeletalMesh(Tab.SelectedMesh))
	{
		if (Tab.PreviewComponent)
		{
			Tab.PreviewComponent->ResetPoseToBindPose();
		}
		Tab.PreviewBounds = Tab.CalculatePreviewBounds();
		Tab.SaveStatusMessage = "Saved to .bin successfully!";
		Tab.SaveStatusTimer = 3.5f;
		return true;
	}
	else
	{
		Tab.SaveStatusMessage = "Failed to save to .bin.";
		Tab.SaveStatusTimer = 3.5f;
		return false;
	}
}

bool FSkeletalMeshAssetSaver::RestoreToOriginalFBXPose()
{
	if (Tab.Animation.BlockEditDuringAnimationPreview())
	{
		return false;
	}

	if (!Tab.SelectedMesh)
	{
		Tab.SaveStatusMessage = "No skeletal mesh selected.";
		Tab.SaveStatusTimer = 3.0f;
		return false;
	}

	if (UAssetManager::Get().RestoreSkeletalMeshToOriginalFBX(Tab.SelectedMesh))
	{
		if (Tab.PreviewComponent)
		{
			Tab.PreviewComponent->ResetPoseToBindPose();
		}
		Tab.PreviewBounds = Tab.CalculatePreviewBounds();
		Tab.SaveStatusMessage = "Restored to original FBX pose successfully!";
		Tab.SaveStatusTimer = 3.5f;
		return true;
	}
	else
	{
		Tab.SaveStatusMessage = "Cannot restore: Original .fbx file does not exist.";
		Tab.SaveStatusTimer = 3.5f;
		return false;
	}
}

bool FSkeletalMeshAssetSaver::SaveAsNewBin(const FString& NewAssetName)
{
	if (Tab.Animation.BlockEditDuringAnimationPreview())
	{
		return false;
	}

	if (!Tab.SelectedMesh || NewAssetName.empty())
	{
		Tab.SaveStatusMessage = "Asset name cannot be empty.";
		Tab.SaveStatusTimer = 3.0f;
		return false;
	}

	// 1. 현재 프리뷰에서 변형된 본 로컬 트랜스폼을 적용한 복사본 MeshData 준비
	FSkeletalMeshData NewMeshData = Tab.SelectedMesh->MeshData;
	if (Tab.PreviewComponent)
	{
		const int32 NumBones = NewMeshData.Bones.Num();
		for (int32 BoneIdx = 0; BoneIdx < NumBones; ++BoneIdx)
		{
			FTransform CurTf;
			if (Tab.PreviewComponent->GetBoneLocalTransform(BoneIdx, CurTf))
			{
				NewMeshData.Bones[BoneIdx].RefPoseTransform = CurTf;
			}
		}
	}

	// 2. AssetManager를 통해 새 .fbx.bin 파일 저장 및 에셋 등록
	USkeletalMesh* NewMesh = UAssetManager::Get().SaveSkeletalMeshAsNewBin(Tab.SelectedMesh, NewAssetName, NewMeshData);
	if (NewMesh)
	{
		Tab.OpenMesh(NewMesh);
		Tab.SaveStatusMessage = "Saved and loaded new asset: " + NewMesh->GetPath();
		Tab.SaveStatusTimer = 4.0f;
		return true;
	}
	else
	{
		Tab.SaveStatusMessage = "Failed to save new .fbx.bin asset.";
		Tab.SaveStatusTimer = 3.5f;
		return false;
	}
}


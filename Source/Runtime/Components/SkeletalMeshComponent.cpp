#include "EnginePCH.h"
#include "SkeletalMeshComponent.h"
#include "Asset/AssetManager.h"
#include "Core/EngineLog.h"
#include "Math/Quat.h"

USkeletalMeshComponent::USkeletalMeshComponent()
{
	// 임시 기본값: Details에서 고르기 전까지 (Step 9에서 제거)
	SkeletalMesh = UAssetManager::GetAssetByPath<USkeletalMesh>("Models/UnrealMan/SKM_Manny_Simple.FBX");
	ResetToRefPose();
}

void USkeletalMeshComponent::ResetToRefPose()
{
	LocalPose.Reset();
	if (!SkeletalMesh)
		return;

	for (const FBoneInfo& Bone : SkeletalMesh->Data.Bones)
	{
		LocalPose.Add(Bone.RefLocal);
	}
}

bool USkeletalMeshComponent::SetBoneLocalRotation(const FString& BoneName, const FRotator& Rotation)
{
	if (!SkeletalMesh)
		return false;

	const TArray<FBoneInfo>& Bones = SkeletalMesh->Data.Bones;
	if (LocalPose.Num() != Bones.Num())
		ResetToRefPose();

	for (int32 i = 0; i < Bones.Num(); ++i)
	{
		if (Bones[i].Name == BoneName)
		{
			// 뼈 자기 축으로 돌린 뒤 부모 기준 자리로 (행벡터: 왼쪽부터 적용)
			LocalPose[i] = Rotation.Quaternion().ToFMatrix() * Bones[i].RefLocal;
			return true;
		}
	}
	return false;
}

void USkeletalMeshComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);

	if (Property.Name == "TestBoneName" || Property.Name == "TestBoneRotation")
	{
		ResetToRefPose();
		if (!SetBoneLocalRotation(TestBoneName, TestBoneRotation))
		{
			HTR_LOG(Warning, "Bone not found: {}", TestBoneName);
		}
		ApplyPose();
	}
}

#pragma once

#include <cmath>

#include "Render/StaticMeshData.h"
#include "Math/Matrix.h"

// FBX에서 구워 낸 SkeletalMesh 데이터. 에디터(FFbxImporter)가 채우고 런타임(AssetManager)이 읽는다.
// 규칙
// 1. SkinWeights[i]는 Mesh.Vertices[i]의 짝 (순서·개수 동일)
// 2. Bones는 부모가 자식보다 앞
// 3. 정점은 모델 공간 (피부를 붙인 순간)
// 4. 스키닝은 행벡터: v' = v * InverseBindPose * BoneTransform

// 정점 하나가 따라갈 수 있는 뼈 수 (UE 임포트 상한과 같음). 안 쓰는 칸은 Weight 0
constexpr int32 MAX_TOTAL_INFLUENCES = 12;

struct FSkinWeight
{
	int32 BoneIndex[MAX_TOTAL_INFLUENCES] = {};
	float Weight[MAX_TOTAL_INFLUENCES] = {};
};

struct FBoneInfo
{
	FString Name;
	int32 ParentIndex = -1;      // 루트는 -1
	FMatrix InverseBindPose;     // 피부를 붙인 순간 뼈 전역 행렬의 역행렬
};

struct FSkeletalMeshData
{
	FStaticMeshData Mesh;              // T-Pose 정점·인덱스·섹션·머티리얼·AABB
	TArray<FSkinWeight> SkinWeights;
	TArray<FBoneInfo> Bones;

	// 메시 검사 + 규칙 1·2 + 가중치 범위·합. 비어 있는 배열은 통과 (뼈 없는 FBX)
	bool Validate(FString& OutError) const
	{
		// 1. 메시
		if (!Mesh.Validate(OutError))
		{
			return false;
		}

		// 2. 규칙 1: 가중치와 정점 짝
		if (!SkinWeights.IsEmpty() && SkinWeights.Num() != Mesh.Vertices.Num())
		{
			OutError = "skin weight count != vertex count";
			return false;
		}

		// 3. 규칙 2: 부모가 먼저
		for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
		{
			if (Bones[BoneIndex].ParentIndex >= BoneIndex)
			{
				OutError = "bone parent must come before child";
				return false;
			}
		}

		// 4. 가중치: 뼈 번호 범위, 합 = 1
		for (const FSkinWeight& Skin : SkinWeights)
		{
			float Sum = 0.0f;
			for (int32 Slot = 0; Slot < MAX_TOTAL_INFLUENCES; ++Slot)
			{
				if (Skin.Weight[Slot] > 0.0f && (Skin.BoneIndex[Slot] < 0 || Skin.BoneIndex[Slot] >= Bones.Num()))
				{
					OutError = "bone index out of range";
					return false;
				}
				Sum += Skin.Weight[Slot];
			}
			if (std::abs(Sum - 1.0f) > 1.0e-3f)
			{
				OutError = "weights do not sum to 1";
				return false;
			}
		}
		return true;
	}
};

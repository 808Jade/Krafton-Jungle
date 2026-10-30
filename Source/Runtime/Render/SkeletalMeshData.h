#pragma once

#include "Containers/Array.h"
#include "Core/EngineString.h"
#include "Math/Matrix.h"

struct FBoneInfo
{
	FString Name;
	int32 ParentIndex = -1; // Root = -1. 항상 자기 인덱스보다 작다
	FMatrix RefLocal = FMatrix::Identity; // 부모에서 본 공간 기준
};

struct FSkeletalMeshData
{
	TArray<FBoneInfo> Bones;
};
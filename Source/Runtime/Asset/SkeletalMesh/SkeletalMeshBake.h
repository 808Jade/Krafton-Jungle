#pragma once

#include "Core/Types.h"
#include "Core/EngineString.h"

struct FSkeletalMeshData;

struct FSkeletalMeshBake
{
public:
	static TUniquePtr<FSkeletalMeshData> ReadBaked(const FString& BinPath);
	static void WriteBaked(const FString& BinPath, const FSkeletalMeshData& Data);
};
#pragma once

#include "Core/EngineString.h"

struct FSkeletalMeshData;

// FBX 파일을 FSkeletalMeshData로 굽는다. FBX SDK를 쓰는 유일한 곳이다(에디터 전용).
class FFbxImporter
{
public:
	static bool Import(const FString& FbxPath, FSkeletalMeshData& OutData);
};
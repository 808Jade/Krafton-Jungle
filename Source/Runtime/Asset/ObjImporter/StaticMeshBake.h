#pragma once

#include <fstream>

#include "Core/Types.h"
#include "Core/EngineString.h"

struct FStaticMeshData;

// Cook이 끝난 FStaticMeshData를 .bin으로 저장하고 읽는다.
class FStaticMeshBake
{
public:
	static TUniquePtr<FStaticMeshData> ReadBaked(const FString& BinPath);
	static void WriteBaked(const FString& BinPath, const FStaticMeshData& Data);

	// 헤더 없이 메시 본문만 읽고 쓴다 (SkeletalMeshBake가 같이 쓴다)
	static bool ReadMeshBody(std::ifstream& In, FStaticMeshData& Data);
	static void WriteMeshBody(std::ofstream& Out, const FStaticMeshData& Data);
};

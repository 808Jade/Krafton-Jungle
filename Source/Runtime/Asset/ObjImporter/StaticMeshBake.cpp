#include "EnginePCH.h"
#include "StaticMeshBake.h"

#include <cstring>
#include <fstream>
#include <type_traits>

#include "Render/StaticMeshData.h"
#include "Core/EngineLog.h"
#include "Asset/BakeIO.h"

static_assert(sizeof(FVertex) == 64);

// 바이트 그대로 읽고 쓰는 타입. 복사 생성자를 직접 작성하면 여기서 컴파일 오류가 난다
static_assert(std::is_trivially_copyable_v<FVertex>);
static_assert(std::is_trivially_copyable_v<FStaticMeshSection>);
static_assert(std::is_trivially_copyable_v<FVector4>);
static_assert(std::is_trivially_copyable_v<FBox>);

static_assert(std::is_trivially_copyable_v<FLODSourceVertex>);

using namespace BakeIO;

namespace
{
	// FStaticMeshData, FVertex, FStaticMeshSection, FStaticMaterialSlot 구조가 바뀌면 올린다
	constexpr uint32 BakeVersion{ 5 };

	struct FBakeHeader
	{
		// 'S'taic 'M'esh 'B'a'K'e
		char Magic[4] = { 'S', 'M', 'B', 'K' };
		uint32 Version = BakeVersion;
	};
}

TUniquePtr<FStaticMeshData> FStaticMeshBake::ReadBaked(const FString& BinPath)
{
	std::ifstream In(BinPath, std::ios::binary);
	if (!In)
	{
		return nullptr;   // .bin이 없음 (처음 로드)
	}

	TUniquePtr<FStaticMeshData> Data = MakeUnique<FStaticMeshData>();

	// 우리 형식이 맞는지, 같은 구조로 저장했는지 확인
	FBakeHeader Header;
	const FBakeHeader Expected;
	if (!ReadPod<FBakeHeader>(In, Header) ||
		std::memcmp(Header.Magic, Expected.Magic, sizeof(Header.Magic)) != 0 ||
		Header.Version != Expected.Version)
	{
		HTR_LOG(Warning, "[BAKE] {}: invalid header or version, re-cooking", BinPath);
		return nullptr;
	}

	if (!ReadMeshBody(In, *Data))
	{
		return nullptr;
	}

	// 바이트 수는 맞아도 내용이 깨졌을 수 있으므로 GPU에 올리기 전에 검사
	FString Error;
	if (!Data->Validate(Error))
	{
		HTR_LOG(Warning, "[BAKE] {}: {}, re-cooking", BinPath, Error);
		return nullptr;
	}

	HTR_LOG(Info, "[BAKE] {}: loaded", BinPath);
	return Data;
}

void FStaticMeshBake::WriteBaked(const FString& BinPath, const FStaticMeshData& Data)
{
	std::ofstream Out(BinPath, std::ios::binary);
	if (!Out)
	{
		HTR_LOG(Warning, "[BAKE] {}: cannot open for writing", BinPath);
		return;
	}

	WritePod<FBakeHeader>(Out, FBakeHeader());
	WriteMeshBody(Out, Data);

	// 스트림은 한 번 실패하면 상태가 유지되므로 마지막에 한 번만 확인
	if (!Out)
	{
		HTR_LOG(Warning, "[BAKE] {}: write failed", BinPath);
	}
}

// 메시 본문: 배열 4개 → 머티리얼 슬롯 → AABB (헤더 다음 바이트부터)
bool FStaticMeshBake::ReadMeshBody(std::ifstream& In, FStaticMeshData& Data)
{
	constexpr uint32 MaxCount = 1000000;

	// 1. 배열 4개
	if (!ReadArray<FVertex>(In, Data.Vertices, MaxCount) ||
		!ReadArray<FLODSourceVertex>(In, Data.LODSourceVertices, MaxCount) ||
		!ReadArray<uint32>(In, Data.Indices, MaxCount) ||
		!ReadArray<FStaticMeshSection>(In, Data.Sections, MaxCount))
	{
		return false;
	}

	// 2. 머티리얼 슬롯
	uint32 MaterialSlotsNum = 0;
	if (!ReadPod<uint32>(In, MaterialSlotsNum))
	{
		return false;
	}
	for (uint32 i = 0; i < MaterialSlotsNum; ++i)
	{
		FStaticMaterialSlot Slot{};
		if (!ReadString(In, Slot.Name) || !ReadPod<FVector4>(In, Slot.BaseColor) || !ReadString(In, Slot.DiffuseTexturePath) || !ReadString(In, Slot.NormalTexturePath)) {
			return false;
		}
		Data.MaterialSlots.Add(std::move(Slot));
	}

	// 3. AABB
	return ReadPod<FBox>(In, Data.AABB);
}

void FStaticMeshBake::WriteMeshBody(std::ofstream& Out, const FStaticMeshData& Data)
{
	// 1. 배열 4개
	WriteArray<FVertex>(Out, Data.Vertices);
	WriteArray<FLODSourceVertex>(Out, Data.LODSourceVertices);
	WriteArray<uint32>(Out, Data.Indices);
	WriteArray<FStaticMeshSection>(Out, Data.Sections);

	// 2. 머티리얼 슬롯
	WritePod<uint32>(Out, Data.MaterialSlots.Num());
	for (const auto& Slot : Data.MaterialSlots)
	{
		WriteString(Out, Slot.Name);
		WritePod<FVector4>(Out, Slot.BaseColor);
		WriteString(Out, Slot.DiffuseTexturePath);
		WriteString(Out, Slot.NormalTexturePath);
	}

	// 3. AABB
	WritePod<FBox>(Out, Data.AABB);
}

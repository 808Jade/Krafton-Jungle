#include "EnginePCH.h"
#include "SkeletalMeshBake.h"

#include <cstring>

#include "Asset/BakeIO.h"
#include "Asset/ObjImporter/StaticMeshBake.h"
#include "Render/SkeletalMeshData.h"
#include "Core/EngineLog.h"

using namespace BakeIO;

// 바이트 그대로 읽고 쓰는 타입
static_assert(std::is_trivially_copyable_v<FSkinWeight>);

namespace
{
	// FSkeletalMeshData 구조나 Importer 출력이 바뀌면 올린다
	constexpr uint32 BakeVersion{ 5 };

	struct FBakeHeader
	{
		// 'S'keletal 'K' 'B'a'K'e
		char Magic[4] = { 'S', 'K', 'B', 'K' };
		uint32 Version = BakeVersion;
	};
}

// 형식: 헤더 → 메시 본문(StaticMesh와 같음) → 가중치 배열 → 뼈(개수 + 이름·부모·InverseBindPose)
TUniquePtr<FSkeletalMeshData> FSkeletalMeshBake::ReadBaked(const FString& BinPath)
{
	std::ifstream In(BinPath, std::ios::binary);
	if (!In)
	{
		return nullptr;   // .bin이 없음 (처음 로드)
	}

	// 1. 우리 형식·같은 버전인지 확인
	FBakeHeader Header;
	const FBakeHeader Expected;
	if (!ReadPod(In, Header) ||
		std::memcmp(Header.Magic, Expected.Magic, sizeof(Header.Magic)) != 0 ||
		Header.Version != Expected.Version)
	{
		HTR_LOG(Warning, "[BAKE] {}: invalid header or version, re-importing", BinPath);
		return nullptr;
	}

	// 2. 메시 본문
	TUniquePtr<FSkeletalMeshData> Data = MakeUnique<FSkeletalMeshData>();
	if (!FStaticMeshBake::ReadMeshBody(In, Data->Mesh))
	{
		return nullptr;
	}

	// 3. 가중치 (숫자만 있는 구조체라 배열 통째로)
	if (!ReadArray<FSkinWeight>(In, Data->SkinWeights, 1000000))
	{
		return nullptr;
	}

	// 4. 뼈
	uint32 BoneCount = 0;
	if (!ReadPod(In, BoneCount) || BoneCount > 4096)
	{
		return nullptr;
	}
	for (uint32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		FBoneInfo Bone;
		if (!ReadString(In, Bone.Name) || !ReadPod(In, Bone.ParentIndex) || !ReadPod(In, Bone.InverseBindPose.M))
		{
			return nullptr;
		}
		Data->Bones.Add(std::move(Bone));
	}

	// 5. 내용 검사 (바이트 수는 맞아도 깨졌을 수 있음)
	FString Error;
	if (!Data->Validate(Error))
	{
		HTR_LOG(Warning, "[BAKE] {}: {}, re-importing", BinPath, Error);
		return nullptr;
	}

	HTR_LOG(Info, "[BAKE] {}: loaded", BinPath);
	return Data;
}

void FSkeletalMeshBake::WriteBaked(const FString& BinPath, const FSkeletalMeshData& Data)
{
	std::ofstream Out(BinPath, std::ios::binary);
	if (!Out)
	{
		HTR_LOG(Warning, "[BAKE] {}: cannot open for writing", BinPath);
		return;
	}

	// 1. 헤더
	WritePod(Out, FBakeHeader());

	// 2. 메시 본문
	FStaticMeshBake::WriteMeshBody(Out, Data.Mesh);

	// 3. 가중치
	WriteArray<FSkinWeight>(Out, Data.SkinWeights);

	// 4. 뼈 (FMatrix는 operator=를 직접 정의해 trivially copyable이 아니라 float[4][4]로 쓴다)
	WritePod(Out, static_cast<uint32>(Data.Bones.Num()));
	for (const FBoneInfo& Bone : Data.Bones)
	{
		WriteString(Out, Bone.Name);
		WritePod(Out, Bone.ParentIndex);
		WritePod(Out, Bone.InverseBindPose.M);
	}

	// 스트림은 한 번 실패하면 상태가 유지되므로 마지막에 한 번만 확인
	if (!Out)
	{
		HTR_LOG(Warning, "[BAKE] {}: write failed", BinPath);
	}
}

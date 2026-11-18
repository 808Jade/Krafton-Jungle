#pragma once

#include "Asset/RenderAsset.h"
#include "Math/Box.h"

class UParticleEmitter;

enum class EParticleSystemLODMethod : uint8
{
	Automatic,
	DirectSet,
	ActivateAutomatic
};

// 파티클 이펙트 에셋. 에미터 목록을 소유한다.
class UParticleSystem : public URenderAsset
{
	DECLARE_CLASS(UParticleSystem, URenderAsset)

public:
	UParticleSystem() = default;
	~UParticleSystem() override = default;

	// .particle 파일의 "System" 레코드. 이미터 -> LOD -> 모듈 트리는 리플렉션 프로퍼티가 아니라 여기서 직접 저장한다.
	// 불러올 때는 새로 만든(비어 있는) 객체에만 호출한다. 기존 이미터는 지우지 않는다.
	using Super::Serialize;
	void Serialize(FStructuredArchive::FRecord Record) override;

	TArray<UParticleEmitter*> Emitters;
	bool	bUseFixedRelativeBoundingBox = false;
	FBox	FixedRelativeBoundingBox{ FVector(-1.0f, -1.0f, -1.0f), FVector(1.0f, 1.0f, 1.0f) };
	TArray<float>				LODDistances;
	EParticleSystemLODMethod	LODMethod = EParticleSystemLODMethod::Automatic;
	float						LODDistanceCheckTime = 0.25f;

	int32 GetLODLevelCount() const { return LODDistances.Num(); }

};

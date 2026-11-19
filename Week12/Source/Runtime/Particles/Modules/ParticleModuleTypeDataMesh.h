#pragma once

#include "Particles/Modules/ParticleModuleTypeDataBase.h"
#include "Particles/ParticleRange.h"

// Mesh 프로퍼티 등록에 완전한 타입이 필요하다.
#include "Render/Mesh.h"

class UParticleModuleTypeDataMesh : public UParticleModuleTypeDataBase
{
	DECLARE_CLASS(UParticleModuleTypeDataMesh, UParticleModuleTypeDataBase)
	REFLECT_START(UParticleModuleTypeDataMesh)
		PROPERTY(Mesh)
		PROPERTY(StartRotation)
		PROPERTY(RotationRate)
	REFLECT_END()

public:
	UParticleModuleTypeDataMesh() = default;

	EDynamicEmitterType GetEmitterType() const override
	{
		return EDynamicEmitterType::Mesh;
	}

	// 회전 값(FVector) 하나를 저장하는 Payload
	int32 GetRequiredPayloadBytes() const override
	{
		return sizeof(FVector);
	}

	// 태어날 때 초기 회전을 Payload에 기록한다.
	void Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle) override;

	// 매 틱 Payload의 회전에 RotationRate * DeltaTime을 누적한다.
	void Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime) override;

	bool NeedsSpawn() const override { return true; }
	bool NeedsUpdate() const override
	{
		return RotationRate.X != 0.0f || RotationRate.Y != 0.0f || RotationRate.Z != 0.0f;
	}

	UStaticMesh* Mesh = nullptr;

	// 초기 회전 범위. 단위는 라디안(FBaseParticle::Rotation과 동일).
	FParticleVectorRange StartRotation;

	// 모든 파티클에 같이 적용되는 회전 속도(라디안/초). 0이면 Update를 돌리지 않는다.
	FVector RotationRate = FVector(0.0f, 0.0f, 0.0f);
};

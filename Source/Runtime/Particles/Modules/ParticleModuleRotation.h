#pragma once

#include "Particles/ParticleModule.h"
#include "Particles/ParticleRange.h"

// 스프라이트 파티클이 태어날 때의 처음 회전 각도를 정하는 모듈.
// Mesh 에미터의 회전은 TypeDataMesh가 따로 payload로 다룬다.
class UParticleModuleRotation : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleRotation, UParticleModule)
	REFLECT_START(UParticleModuleRotation)
		PROPERTY(StartRotation)
	REFLECT_END()

public:
	UParticleModuleRotation() = default;

	void Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle) override;
	bool NeedsSpawn() const override { return true; }

	// 처음 회전 각도 범위. 단위는 라디안(FBaseParticle::Rotation과 동일).
	FParticleFloatRange StartRotation;
};

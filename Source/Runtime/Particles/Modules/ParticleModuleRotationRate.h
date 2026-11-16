#pragma once

#include "Particles/ParticleModule.h"
#include "Particles/ParticleRange.h"

// 스프라이트 파티클이 태어날 때의 회전 속도를 정하는 모듈.
// 실제로 각도를 돌리는 일은 A의 IntegrateParticles가 한다 (Rotation += RotationRate * dt).
class UParticleModuleRotationRate : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleRotationRate, UParticleModule)

public:
	UParticleModuleRotationRate() = default;

	void Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle) override;
	bool NeedsSpawn() const override { return true; }

	// 처음 회전 속도 범위. 단위는 라디안/초.
	FParticleFloatRange StartRotationRate;
};

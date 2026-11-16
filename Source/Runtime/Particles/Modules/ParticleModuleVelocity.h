#pragma once

#include "Particles/ParticleModule.h"
#include "Particles/ParticleRange.h"

class UParticleModuleVelocity : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleVelocity, UParticleModule)
	REFLECT_START(UParticleModuleVelocity)
		PROPERTY(StartVelocity)
	REFLECT_END()

public:
	UParticleModuleVelocity() = default;

	void Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle) override;
	bool NeedsSpawn() const override { return true; }

	FParticleVectorRange StartVelocity;
};

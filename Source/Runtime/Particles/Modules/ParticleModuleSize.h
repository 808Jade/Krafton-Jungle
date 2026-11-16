#pragma once

#include "Particles/ParticleModule.h"
#include "Particles/ParticleRange.h"

class UParticleModuleSize : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleSize, UParticleModule)
	REFLECT_START(UParticleModuleSize)
		PROPERTY(StartSize)
		PROPERTY(bUseSizeOverLife)
		PROPERTY(EndSize)
	REFLECT_END()

public:
	UParticleModuleSize() = default;

	void Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle) override;

	void Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime) override;

	bool NeedsSpawn() const override { return true; }
	bool NeedsUpdate() const override { return bUseSizeOverLife; }

	FParticleVectorRange StartSize = FParticleVectorRange(FVector(1.0f, 1.0f, 1.0f));

	// true이면 수명에 따라 BaseSize에서 EndSize로 선형 보간한다.
	bool bUseSizeOverLife = false;
	FVector EndSize = FVector(1.0f, 1.0f, 1.0f);
};

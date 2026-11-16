#pragma once

#include "Particles/ParticleModule.h"
#include "Particles/ParticleRange.h"

class UParticleModuleColor : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleColor, UParticleModule)
	REFLECT_START(UParticleModuleColor)
		PROPERTY(StartColor)
		PROPERTY(bUseColorOverLife)
		PROPERTY(EndColor)
	REFLECT_END()

public:
	UParticleModuleColor() = default;

	void Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle) override;

	void Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime) override;

	bool NeedsSpawn() const override { return true; }
	bool NeedsUpdate() const override { return bUseColorOverLife; }

	FParticleColorRange StartColor;

	// true이면 수명에 따라 BaseColor에서 EndColor로 선형 보간한다.
	bool bUseColorOverLife = false;
	FLinearColor EndColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.0f);
};

#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleSize.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleHelper.h"

void UParticleModuleSize::Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle)
{
	const FVector S = StartSize.Sample();
	Particle.Size += S;
	Particle.BaseSize += S;
}

void UParticleModuleSize::Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime)
{
	if (!bUseSizeOverLife) return; // 리셋으로 이미 Size == BaseSize라 할 일이 없음

	for (int32 i = 0; i < Owner.ActiveParticles; ++i)
	{
		DECLARE_PARTICLE_PTR(Particle, Owner.ParticleData + Owner.ParticleStride * Owner.ParticleIndices[i]);
		if (Particle->Flags & STATE_Particle_Freeze) continue;

		const float T = std::clamp(Particle->RelativeTime, 0.0f, 1.0f);
		Particle->Size = Particle->BaseSize + (EndSize - Particle->BaseSize) * T; // 선형 보간
	}
}

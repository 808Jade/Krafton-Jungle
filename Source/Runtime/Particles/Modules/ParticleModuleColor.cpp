#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleColor.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleHelper.h"

void UParticleModuleColor::Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle)
{
	const FLinearColor C = StartColor.Sample();
	Particle.Color = C;
	Particle.BaseColor = C;
}

void UParticleModuleColor::Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime)
{
	if (!bUseColorOverLife) return;

	for (int32 i = 0; i < Owner.ActiveParticles; ++i)
	{
		DECLARE_PARTICLE_PTR(Particle, Owner.ParticleData + Owner.ParticleStride * Owner.ParticleIndices[i]);
		if (Particle->Flags & STATE_Particle_Freeze) continue;

		const float T = std::clamp(Particle->RelativeTime, 0.0f, 1.0f);
		Particle->Color.R = Particle->BaseColor.R + (EndColor.R - Particle->BaseColor.R) * T;
		Particle->Color.G = Particle->BaseColor.G + (EndColor.G - Particle->BaseColor.G) * T;
		Particle->Color.B = Particle->BaseColor.B + (EndColor.B - Particle->BaseColor.B) * T;
		Particle->Color.A = Particle->BaseColor.A + (EndColor.A - Particle->BaseColor.A) * T;
	}
}

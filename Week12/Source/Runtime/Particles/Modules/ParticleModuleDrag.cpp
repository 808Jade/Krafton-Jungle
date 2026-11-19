#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleDrag.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleHelper.h"

void UParticleModuleDrag::Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime)
{
	if (DragCoefficient <= 0.0f || DeltaTime <= 0.0f)
	{
		return;
	}

	const float Factor = std::exp(-DragCoefficient * DeltaTime);

	for (int32 i = 0; i < Owner.ActiveParticles; ++i)
	{
		DECLARE_PARTICLE_PTR(Particle, Owner.ParticleData + Owner.ParticleStride * Owner.ParticleIndices[i]);
		if (Particle->Flags & STATE_Particle_Freeze) continue;

		Particle->BaseVelocity *= Factor;
		Particle->Velocity *= Factor;
	}
}

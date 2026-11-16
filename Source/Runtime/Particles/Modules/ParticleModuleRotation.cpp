#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleRotation.h"

// 태어날 때 처음 회전 각도를 뽑아서 Rotation에 더한다. (Velocity/Location과 같은 += 패턴)
void UParticleModuleRotation::Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle)
{
	Particle.Rotation += StartRotation.Sample();
}

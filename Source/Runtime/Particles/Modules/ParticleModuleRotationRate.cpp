#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleRotationRate.h"

// 회전 속도를 뽑아서 RotationRate와 BaseRotationRate에 함께 더한다.
// 매 틱 리셋이 RotationRate를 BaseRotationRate로 되돌리므로 Base에도 넣어야 속도가 유지된다.
void UParticleModuleRotationRate::Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle)
{
	const float Rate = StartRotationRate.Sample();
	Particle.RotationRate += Rate;
	Particle.BaseRotationRate += Rate;
}

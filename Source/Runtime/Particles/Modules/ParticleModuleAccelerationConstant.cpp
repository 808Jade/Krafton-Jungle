#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleAccelerationConstant.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleHelper.h"

void UParticleModuleAccelerationConstant::Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime)
{
	// 시뮬레이션 공간의 가속도. 월드 기준이고 로컬 스페이스라면 월드→시뮬레이션 방향으로 한 번만 바꾼다.
	FVector SimAcceleration = Acceleration;
	if (bAlwaysInWorldSpace && Owner.bUseLocalSpace)
	{
		SimAcceleration = Owner.SimulationToWorld.Inverse().TransformVector(Acceleration);
	}

	const FVector DeltaVelocity = SimAcceleration * DeltaTime;

	for (int32 i = 0; i < Owner.ActiveParticles; ++i)
	{
		DECLARE_PARTICLE_PTR(Particle, Owner.ParticleData + Owner.ParticleStride * Owner.ParticleIndices[i]);
		if (Particle->Flags & STATE_Particle_Freeze) continue;

		Particle->BaseVelocity += DeltaVelocity;
		Particle->Velocity += DeltaVelocity;
	}
}

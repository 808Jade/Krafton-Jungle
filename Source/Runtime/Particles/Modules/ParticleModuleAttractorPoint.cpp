#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleAttractorPoint.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleHelper.h"

namespace
{
	// 점과 이보다 가까운 파티클은 방향을 정할 수 없어 건드리지 않는다. (m)
	constexpr float MinDistance = 1e-4f;
}

void UParticleModuleAttractorPoint::Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime)
{
	if (Range <= 0.0f || Strength == 0.0f || DeltaTime <= 0.0f)
	{
		return;
	}

	// 파티클 위치는 시뮬레이션 공간이다. 끌어당기는 점도 같은 공간으로 맞춘다.
	FVector SimPoint;
	if (bUseWorldSpacePosition)
	{
		SimPoint = Owner.bUseLocalSpace
			? Owner.SimulationToWorld.Inverse().TransformPosition(Position)
			: Position;
	}
	else
	{
		// 이미터 기준 위치 → 시뮬레이션 공간. 로컬 스페이스면 항등이고, 월드 스페이스면 컴포넌트 변환이다.
		SimPoint = Owner.EmitterToSimulation.TransformPosition(Position);
	}

	for (int32 i = 0; i < Owner.ActiveParticles; ++i)
	{
		DECLARE_PARTICLE_PTR(Particle, Owner.ParticleData + Owner.ParticleStride * Owner.ParticleIndices[i]);
		if (Particle->Flags & STATE_Particle_Freeze) continue;

		const FVector ToPoint = SimPoint - Particle->Location;
		const float Distance = ToPoint.Length();
		if (Distance < MinDistance || Distance > Range) continue;

		float Magnitude = Strength;
		if (bStrengthByDistance)
		{
			Magnitude *= 1.0f - Distance / Range;
		}

		const FVector DeltaVelocity = ToPoint * (Magnitude * DeltaTime / Distance);
		Particle->BaseVelocity += DeltaVelocity;
		Particle->Velocity += DeltaVelocity;
	}
}

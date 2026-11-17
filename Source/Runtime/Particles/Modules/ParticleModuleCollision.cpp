#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleCollision.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleHelper.h"
#include "Components/ParticleSystemComponent.h"
#include "Collision/HitResult.h"
#include "Engine/World.h"

namespace
{
	// 표면에 닿은 파티클을 법선 방향으로 살짝 띄워 다음 틱에 같은 면에 다시 걸리지 않게 한다. (m)
	constexpr float SurfaceOffset = 0.005f;
	// 이동 구간이 이보다 짧으면 검사하지 않는다. (m)
	constexpr float MinTraceDistance = 1e-5f;

	FVector Reflect(const FVector& V, const FVector& Normal)
	{
		return V - Normal * (2.0f * V.Dot(Normal));
	}
}

void UParticleModuleCollision::Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime)
{
	UParticleSystemComponent* Component = Owner.Component;
	UWorld* World = Component ? Component->GetWorld() : nullptr;
	if (!World || DeltaTime <= 0.0f)
	{
		return;
	}

	const AActor* IgnoreActor = Component->GetOwner();

	// 파티클 위치는 시뮬레이션 공간이다. 로컬 스페이스면 월드로 바꿔 쏘고, 결과는 다시 시뮬레이션 공간으로 되돌린다.
	const bool bLocalSpace = Owner.bUseLocalSpace;
	const FMatrix SimulationToWorld = Owner.SimulationToWorld;
	const FMatrix WorldToSimulation = bLocalSpace ? SimulationToWorld.Inverse() : FMatrix::Identity;

	const float Damping = std::max(DampingFactor, 0.0f);
	const int32 MaxCount = std::max(MaxCollisions, 1);

	// 파티클을 지울 수 있으므로 뒤에서부터 돈다. (AgeAndKillParticles와 같은 방식)
	for (int32 i = Owner.ActiveParticles - 1; i >= 0; --i)
	{
		DECLARE_PARTICLE_PTR(Particle, Owner.ParticleData + Owner.ParticleStride * Owner.ParticleIndices[i]);
		if (Particle->Flags & STATE_Particle_CollisionIgnoreCheck)
		{
			continue;
		}

		const FVector Delta = Particle->Velocity * DeltaTime;
		if (Delta.Length() < MinTraceDistance)
		{
			continue;
		}

		FVector Start = Particle->Location;
		FVector End = Start + Delta;
		if (bLocalSpace)
		{
			Start = SimulationToWorld.TransformPosition(Start);
			End = SimulationToWorld.TransformPosition(End);
		}

		FHitResult Hit;
		if (!World->LineTraceSegment(Start, End, Hit, IgnoreActor))
		{
			continue;
		}

		FVector HitPoint = Hit.ImpactPoint;
		FVector Normal = Hit.ImpactNormal;
		if (bLocalSpace)
		{
			HitPoint = WorldToSimulation.TransformPosition(HitPoint);
			Normal = WorldToSimulation.TransformVector(Normal).GetSafeNormal(1e-8f, Normal);
		}

		int32& UsedCollisions = *reinterpret_cast<int32*>(reinterpret_cast<uint8*>(Particle) + PayloadOffset);
		++UsedCollisions;
		Particle->Flags |= STATE_Particle_CollisionHasOccurred;
		Particle->Location = HitPoint + Normal * SurfaceOffset;

		const bool bFinalCollision = UsedCollisions >= MaxCount;
		if (bFinalCollision && CollisionCompletionOption == EParticleCollisionComplete::Kill)
		{
			Owner.KillParticle(i);
			continue;
		}
		if (bFinalCollision && CollisionCompletionOption == EParticleCollisionComplete::Freeze)
		{
			Particle->Velocity = FVector(0.0f, 0.0f, 0.0f);
			Particle->BaseVelocity = FVector(0.0f, 0.0f, 0.0f);
			Particle->RotationRate = 0.0f;
			Particle->BaseRotationRate = 0.0f;
			Particle->Flags |= STATE_Particle_Freeze;
			continue;
		}

		Particle->Velocity = Reflect(Particle->Velocity, Normal) * Damping;
		Particle->BaseVelocity = Reflect(Particle->BaseVelocity, Normal) * Damping;

		if (bFinalCollision) // HaltCollisions: 마지막 충돌은 튕기고 그 뒤로는 검사하지 않는다.
		{
			Particle->Flags |= STATE_Particle_IgnoreCollisions;
		}
	}
}

#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleEventGenerator.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleHelper.h"
#include "Components/ParticleSystemComponent.h"
#include "Math/Noise.h"

namespace
{
	// 파티클 payload. 죽음 이벤트를 보냈으면 DeathSent가 1이다.
	struct FEventGeneratorPayload
	{
		float DeathSent;
		float TrailTimer;
	};
}

void UParticleModuleEventGenerator::Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle)
{
	FEventGeneratorPayload& Payload = *reinterpret_cast<FEventGeneratorPayload*>(reinterpret_cast<uint8*>(&Particle) + PayloadOffset);
	Payload.DeathSent = 0.0f;
	// 첫 꼬리는 태어난 뒤 한 간격이 지나서 만들어진다.
	Payload.TrailTimer = TrailRate > 0.0f ? 1.0f / TrailRate : 0.0f;
}

void UParticleModuleEventGenerator::Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime)
{
	UParticleSystemComponent* Component = Owner.Component;
	if (!Component || DeltaTime <= 0.0f || !Component->WantsCollisionEvents())
	{
		return;
	}

	const bool bDeath = !DeathEventName.empty();
	const bool bTrail = !TrailEventName.empty() && TrailRate > 0.0f;
	if (!bDeath && !bTrail)
	{
		return;
	}

	const bool bLocalSpace = Owner.bUseLocalSpace;
	const FMatrix SimulationToWorld = Owner.SimulationToWorld;
	const float TrailInterval = bTrail ? 1.0f / TrailRate : 0.0f;
	int32 EventsThisFrame = 0;

	auto Emit = [&](const FString& Name, const FBaseParticle& Particle)
	{
		FParticleCollisionEvent Event;
		Event.EventName = Name;
		Event.EmitterTime = Owner.EmitterTime;
		Event.ParticleRelativeTime = Particle.RelativeTime;
		Event.CollisionCount = 0;
		Event.Location = bLocalSpace ? SimulationToWorld.TransformPosition(Particle.Location) : Particle.Location;
		Event.Velocity = bLocalSpace ? SimulationToWorld.TransformVector(Particle.Velocity) : Particle.Velocity;
		Event.Normal = Event.Velocity.GetSafeNormal(1e-6f, FVector(0.0f, 0.0f, 1.0f));
		Component->QueueCollisionEvent(std::move(Event));
	};

	for (int32 i = 0; i < Owner.ActiveParticles; ++i)
	{
		DECLARE_PARTICLE_PTR(Particle, Owner.ParticleData + Owner.ParticleStride * Owner.ParticleIndices[i]);
		FEventGeneratorPayload& Payload = *reinterpret_cast<FEventGeneratorPayload*>(reinterpret_cast<uint8*>(Particle) + PayloadOffset);

		const bool bCanEmit = MaxEventsPerFrame <= 0 || EventsThisFrame < MaxEventsPerFrame;

		if (bTrail)
		{
			Payload.TrailTimer -= DeltaTime;
			if (Payload.TrailTimer <= 0.0f)
			{
				// 한 틱에 하나만 만들고, 밀린 시간은 한 간격까지만 이월한다. (프레임이 느릴 때 이벤트가 몰리지 않게)
				Payload.TrailTimer = std::min(Payload.TrailTimer + TrailInterval, TrailInterval);
				if (bCanEmit)
				{
					++EventsThisFrame;
					Emit(TrailEventName, *Particle);
				}
			}
		}

		// 다음 틱의 AgeAndKillParticles에서 수명이 끝나면 지금이 마지막 갱신이다. 그때 한 번만 보낸다.
		if (bDeath && Payload.DeathSent == 0.0f && Particle->RelativeTime + Particle->OneOverMaxLifetime * DeltaTime >= 1.0f)
		{
			Payload.DeathSent = 1.0f;
			if (MaxEventsPerFrame <= 0 || EventsThisFrame < MaxEventsPerFrame)
			{
				if (DeathEventChance >= 1.0f || FMath::FRand() < DeathEventChance)
				{
					++EventsThisFrame;
					Emit(DeathEventName, *Particle);
				}
			}
		}
	}
}

#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleEventReceiverSpawn.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Components/ParticleSystemComponent.h"

void UParticleModuleEventReceiverSpawn::Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime)
{
	UParticleSystemComponent* Component = Owner.Component;
	if (!Component || SpawnCount <= 0 || Owner.bHaltSpawning)
	{
		return;
	}

	const TArray<FParticleCollisionEvent>& Events = Component->GetReceivedCollisionEvents();
	if (Events.IsEmpty())
	{
		return;
	}

	// 이벤트의 위치와 방향은 월드 좌표다. 로컬 스페이스 이미터는 시뮬레이션 공간으로 바꿔서 스폰한다.
	const bool bLocalSpace = Owner.bUseLocalSpace;
	const FMatrix WorldToSimulation = bLocalSpace ? Owner.SimulationToWorld.Inverse() : FMatrix::Identity;
	const FVector4 Origin = Owner.EmitterToSimulation.GetOrigin();
	const FVector EmitterOrigin(Origin.X, Origin.Y, Origin.Z);

	for (const FParticleCollisionEvent& Event : Events)
	{
		if (!EventName.empty() && EventName != Event.EventName)
		{
			continue;
		}

		FVector SpawnLocation = EmitterOrigin;
		if (bSpawnAtEventLocation)
		{
			const FVector WorldLocation = Event.Location + Event.Normal * SurfaceOffset;
			SpawnLocation = bLocalSpace ? WorldToSimulation.TransformPosition(WorldLocation) : WorldLocation;
		}

		const FVector WorldVelocity = Event.Normal * NormalSpeed.Sample();
		const FVector SpawnVelocity = bLocalSpace ? WorldToSimulation.TransformVector(WorldVelocity) : WorldVelocity;

		// 월드 스페이스 이미터는 컴포넌트가 움직인 만큼 새 파티클 위치를 보정한다. 이벤트 위치는 이미 정확한 월드 좌표라서,
		// 컴포넌트가 움직이는 중이어도 어긋나지 않게 잠시 보정을 끈다.
		const FVector SavedOldLocation = Owner.OldLocation;
		Owner.OldLocation = Owner.Location;
		Owner.SpawnParticles(SpawnCount, 0.0f, 0.0f, SpawnLocation, SpawnVelocity);
		Owner.OldLocation = SavedOldLocation;
	}
}

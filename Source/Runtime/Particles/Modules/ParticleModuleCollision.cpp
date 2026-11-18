#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleCollision.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/Modules/ParticleModuleTypeDataMesh.h"
#include "Render/StaticMeshData.h"
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

	// 파티클 반경 계산에 쓰는 값. 렌더러가 Size에 컴포넌트 월드 스케일을 곱해서 그리므로 같은 스케일을 쓴다.
	const float RadiusScale = std::max(ParticleRadiusScale, 0.0f);
	const FVector ComponentScale = Component->GetWorldScale3D().GetAbs();
	const float MaxComponentScale = std::max({ ComponentScale.X, ComponentScale.Y, ComponentScale.Z });
	// 메시 파티클은 Size가 메시에 곱해지는 스케일이다. 메시의 가장 큰 변을 기준 길이로 쓴다. (스프라이트는 0)
	float MeshExtent = 0.0f;
	if (RadiusScale > 0.0f && Owner.SpriteTemplate && Owner.SpriteTemplate->EmitterType == EDynamicEmitterType::Mesh)
	{
		const FParticleMeshEmitterInstance& MeshOwner = static_cast<const FParticleMeshEmitterInstance&>(Owner);
		if (MeshOwner.MeshTypeData && MeshOwner.MeshTypeData->Mesh)
		{
			const FBox& Box = MeshOwner.MeshTypeData->Mesh->GetMeshData().AABB;
			const FVector Extent = Box.Max - Box.Min;
			MeshExtent = std::max({ Extent.X, Extent.Y, Extent.Z });
		}
	}

	// 파티클을 지울 수 있으므로 뒤에서부터 돈다. (AgeAndKillParticles와 같은 방식)
	for (int32 i = Owner.ActiveParticles - 1; i >= 0; --i)
	{
		DECLARE_PARTICLE_PTR(Particle, Owner.ParticleData + Owner.ParticleStride * Owner.ParticleIndices[i]);
		if (Particle->Flags & STATE_Particle_CollisionIgnoreCheck)
		{
			continue;
		}

		const FVector Delta = Particle->Velocity * DeltaTime;
		const float MoveLength = Delta.Length();
		if (MoveLength < MinTraceDistance)
		{
			continue;
		}

		// 파티클 반경(월드 길이). 크기를 모르는 메시나 RadiusScale 0이면 점으로 본다.
		float Radius = 0.0f;
		if (RadiusScale > 0.0f)
		{
			const FVector AbsSize = Particle->Size.GetAbs();
			const float SizeLength = MeshExtent > 0.0f
				? MeshExtent * std::max({ AbsSize.X, AbsSize.Y, AbsSize.Z })
				: std::max(AbsSize.X, AbsSize.Y);
			Radius = 0.5f * SizeLength * MaxComponentScale * RadiusScale;
		}

		// 이번 틱에 움직일 구간(시뮬레이션 공간). 로컬 스페이스면 월드로 바꿔서 검사한다.
		FVector Start = Particle->Location;
		FVector End = Start + Delta;
		if (bLocalSpace)
		{
			Start = SimulationToWorld.TransformPosition(Start);
			End = SimulationToWorld.TransformPosition(End);
		}

		// 반경이 있으면 구를 쓸어서(sweep) 검사한다. 중심 레이 하나로는 곡면이나 모서리에서 가장자리가 먼저 닿는 것을 놓쳐 파고들고 튄다.
		// 반경이 0이면 크기 없는 점이라 선분 레이로 충분하다.
		FHitResult Hit;
		const bool bHit = Radius > 0.0f
			? World->SweepSphereSingle(Start, End, Radius, Hit, IgnoreActor)
			: World->LineTraceSegment(Start, End, Hit, IgnoreActor);
		if (!bHit)
		{
			continue;
		}

		// 구 스윕은 닿는 순간의 중심(Location)을, 점 검사는 표면 위의 충돌점(ImpactPoint)이 곧 중심이다.
		FVector ContactCenter = Radius > 0.0f ? Hit.Location : Hit.ImpactPoint;
		FVector Normal = Hit.ImpactNormal;
		if (bLocalSpace)
		{
			ContactCenter = WorldToSimulation.TransformPosition(ContactCenter);
			Normal = WorldToSimulation.TransformVector(Normal).GetSafeNormal(1e-8f, Normal);
		}

		// 이미 표면에서 멀어지는 중이면 충돌로 치지 않는다. (겹쳐서 빠져나오는 중이거나 모서리를 스치는 경우)
		if (Particle->Velocity.Dot(Normal) >= 0.0f)
		{
			continue;
		}

		int32& UsedCollisions = *reinterpret_cast<int32*>(reinterpret_cast<uint8*>(Particle) + PayloadOffset);
		++UsedCollisions;
		Particle->Flags |= STATE_Particle_CollisionHasOccurred;

		// 닿는 순간의 중심에서 법선 방향으로 조금만 띄워 다음 틱에 같은 면에 다시 걸리지 않게 한다.
		Particle->Location = ContactCenter + Normal * SurfaceOffset;

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

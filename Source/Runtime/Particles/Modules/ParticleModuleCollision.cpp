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
	// 비스듬히 닿을수록 구가 면에 닿기 전에 중심선이 면을 먼저 만난다. 그 거리는 반경 / cos(입사각)이다.
	// 입사각이 이 값(cos)보다 더 눕는 경우는 이 값으로 잘라서 계산하고, 선분도 그만큼만 늘린다.
	constexpr float MinContactCosine = 0.25f;

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

		// 선분 끝을 반경만큼 더 늘려, 파티클 가장자리가 닿는 순간을 중심선 하나로 잡는다.
		// 비스듬히 닿는 경우까지 잡으려고 반경 / MinContactCosine만큼 늘리고, 실제로 이번 틱에 닿는지는 아래에서 다시 거른다.
		const FVector Direction = Delta * (1.0f / MoveLength);
		const FVector SimStart = Particle->Location;
		FVector Start = SimStart;
		FVector End = Start + Direction * (MoveLength + Radius / MinContactCosine);
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

		// 중심선이 면을 만나기까지의 거리 t에서, 구가 면에 닿는 지점까지의 이동 거리 s를 구한다.
		// 면과의 거리가 반경이 되는 지점은 중심선이 면을 만나기 Radius / cos(입사각) 전이다.
		const float Cosine = std::max(-Direction.Dot(Normal), MinContactCosine);
		const float ContactDistance = std::max(Hit.Distance - Radius / Cosine, 0.0f);
		if (ContactDistance > MoveLength + 1e-4f) // 끝점에 정확히 닿는 경우가 부동소수 오차로 걸러지지 않게 여유를 둔다.
		{
			continue; // 이번 틱 안에는 가장자리가 면에 닿지 않는다.
		}

		int32& UsedCollisions = *reinterpret_cast<int32*>(reinterpret_cast<uint8*>(Particle) + PayloadOffset);
		++UsedCollisions;
		Particle->Flags |= STATE_Particle_CollisionHasOccurred;

		// 원래 가던 경로 위의 접촉 지점에 놓는다. (면 위의 충돌점으로 옮기면 비스듬한 충돌에서 앞으로 튄다)
		// 이미 면에 반경보다 가까우면(겹침) 법선 방향으로만 밀어내 반경 + 오프셋 거리를 확보한다.
		FVector ContactCenter = SimStart + Direction * ContactDistance;
		const float RequiredClearance = Radius + SurfaceOffset;
		const float Clearance = (ContactCenter - HitPoint).Dot(Normal);
		if (Clearance < RequiredClearance)
		{
			ContactCenter += Normal * (RequiredClearance - Clearance);
		}
		Particle->Location = ContactCenter;

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

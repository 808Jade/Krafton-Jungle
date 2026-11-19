#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleTypeDataMesh.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleHelper.h"

namespace
{
	// Payload는 FBaseParticle 뒤에 붙으므로 그 앞쪽 오프셋은 유효하지 않다.
	bool IsValidPayloadOffset(int32 PayloadOffset)
	{
		return PayloadOffset >= static_cast<int32>(sizeof(FBaseParticle));
	}
}

// 파티클이 태어날 때, 초기 회전값을 뽑아서 그 파티클의 payload 칸에 적어 둔다.
void UParticleModuleTypeDataMesh::Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle)
{
	if (!IsValidPayloadOffset(PayloadOffset))
	{
		return;
	}

	const FVector Rotation = StartRotation.Sample();
	std::memcpy(reinterpret_cast<uint8*>(&Particle) + PayloadOffset, &Rotation, sizeof(FVector));
}

// 살아 있는 파티클 전부의 payload 회전값에 RotationRate × DeltaTime을 더해서 메시를 돌린다.
void UParticleModuleTypeDataMesh::Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime)
{
	if (!IsValidPayloadOffset(PayloadOffset))
	{
		return;
	}

	for (int32 i = 0; i < Owner.ActiveParticles; ++i)
	{
		DECLARE_PARTICLE_PTR(Particle, Owner.ParticleData + Owner.ParticleStride * Owner.ParticleIndices[i]);
		if (Particle->Flags & STATE_Particle_Freeze) continue;

		// ResetParticleParameters는 Payload를 되돌리지 않으므로 Base 없이 직접 누적한다.
		uint8* PayloadPtr = reinterpret_cast<uint8*>(Particle) + PayloadOffset;
		FVector Rotation;
		std::memcpy(&Rotation, PayloadPtr, sizeof(FVector));
		Rotation += RotationRate * DeltaTime;
		std::memcpy(PayloadPtr, &Rotation, sizeof(FVector));
	}
}

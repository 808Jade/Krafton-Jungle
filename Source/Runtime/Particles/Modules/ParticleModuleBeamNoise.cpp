#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleBeamNoise.h"

#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleEmitterInstance.h"

#include <algorithm>
#include <cstring>

namespace
{
	bool CanUseNoise(
	    const FParticleEmitterInstance& Owner,
	    int32 Offset)
	{
		return Owner.SpriteTemplate &&
		       Owner.SpriteTemplate->EmitterType ==
		           EDynamicEmitterType::Beam &&
		       Offset >= static_cast<int32>(sizeof(FBaseParticle)) &&
		       Offset <= Owner.ParticleSize -
		                     static_cast<int32>(sizeof(FBeamNoisePayload));
	}

	uint32 HashNoise(uint32 Value)
	{
		Value ^= Value >> 16;
		Value *= 0x7feb352du;
		Value ^= Value >> 15;
		Value *= 0x846ca68bu;
		Value ^= Value >> 16;
		return Value;
	}

	float SignedNoise(uint32 Value)
	{
		const uint32 Bits = HashNoise(Value) & 0x00ffffffu;

		return static_cast<float>(Bits) / 16777215.0f * 2.0f - 1.0f;
	}

	void GenerateOffsets(
	    const FBeamNoisePayload& Payload,
	    uint32 Step,
	    FVector* OutOffsets)
	{
		for (int32 Index = 0;
		     Index < MaxBeamNoisePoints;
		     ++Index)
		{
			OutOffsets[Index] = FVector::ZeroVector;
		}

		// 시작점과 끝점은 항상 0.
		for (int32 Index = 1;
		     Index < Payload.SegmentCount;
		     ++Index)
		{
			const uint32 Key =
			    Payload.Seed ^
			    HashNoise(Step + 0x9e3779b9u) ^
			    HashNoise(static_cast<uint32>(Index));

			OutOffsets[Index] = FVector(
			    SignedNoise(Key),
			    SignedNoise(Key ^ 0x68bc21ebu),
			    0.0f);
		}
	}
}

void UParticleModuleBeamNoise::Spawn(
    FParticleEmitterInstance& Owner,
    int32 PayloadOffset,
    float SpawnTime,
    FBaseParticle& Particle)
{
	if (!CanUseNoise(Owner, PayloadOffset))
	{
		return;
	}

	FBeamNoisePayload Payload{};

	// 일반 모듈 Spawn 시점에는 PostSpawn의 counter 증가 전이다.
	Payload.Seed = HashNoise(
	    static_cast<uint32>(RandomSeed) ^
	    HashNoise(Owner.ParticleCounter));

	Payload.SegmentCount = std::clamp(
	    NoiseSegmentCount,
	    1,
	    MaxBeamNoiseSegments);

	GenerateOffsets(Payload, 0, Payload.FromOffsets);

	// A단계에서는 시간 변화가 없으므로 같은 값을 넣는다.
	std::memcpy(
	    Payload.ToOffsets,
	    Payload.FromOffsets,
	    sizeof(Payload.FromOffsets));

	std::memcpy(
	    reinterpret_cast<uint8*>(&Particle) + PayloadOffset,
	    &Payload,
	    sizeof(Payload));
}

void UParticleModuleBeamNoise::Update(
    FParticleEmitterInstance& Owner,
    int32 PayloadOffset,
    float DeltaTime)
{
	// A단계: 생성 시 모양 유지.
	// B단계에서 교체한다.
}
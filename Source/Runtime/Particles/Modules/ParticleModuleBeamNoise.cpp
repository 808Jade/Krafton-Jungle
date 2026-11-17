#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleBeamNoise.h"

#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleEmitterInstance.h"

#include <algorithm>
#include <cmath>
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

	void AdvanceNoise(
	    FBeamNoisePayload& Payload,
	    float DeltaTime,
	    float UpdateInterval)
	{
		if (UpdateInterval <= 0.0f || DeltaTime <= 0.0f)
		{
			return;
		}

		const double Interval =
		    static_cast<double>(std::max(UpdateInterval, 0.01f));

		const double Total =
		    static_cast<double>(Payload.Elapsed) +
		    static_cast<double>(DeltaTime);

		const double PassedSteps = std::floor(Total / Interval);

		Payload.Elapsed =
		    static_cast<float>(std::fmod(Total, Interval));

		if (PassedSteps >= 1.0)
		{
			// 긴 DeltaTime에서도 while 반복 없이 목표 Step으로 이동.
			// uint32 Step의 wrap을 명시적으로 처리한다.
			const uint32 Advance = static_cast<uint32>(
			    std::fmod(PassedSteps, 4294967296.0));

			Payload.Step += Advance;

			GenerateOffsets(
			    Payload,
			    Payload.Step,
			    Payload.FromOffsets);

			GenerateOffsets(
			    Payload,
			    Payload.Step + 1u,
			    Payload.ToOffsets);
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
	
	GenerateOffsets(Payload, 1, Payload.ToOffsets);
	// 프레임 중간에 생성된 경우 생성 이후 경과 시간을 반영.
	AdvanceNoise(
	    Payload,
	    std::max(SpawnTime, 0.0f),
	    UpdateInterval);

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
	if (!CanUseNoise(Owner, PayloadOffset) ||
	    UpdateInterval <= 0.0f ||
	    DeltaTime <= 0.0f)
	{
		return;
	}

	for (int32 Index = 0; Index < Owner.ActiveParticles; ++Index)
	{
		const uint16 Slot = Owner.ParticleIndices[Index];

		auto* Particle = reinterpret_cast<FBaseParticle*>(
		    Owner.ParticleData + Slot * Owner.ParticleStride);

		if (Particle->RelativeTime >= 1.0f ||
		    (Particle->Flags &
		     (STATE_Particle_Freeze |
		      STATE_Particle_FreezeTranslation)))
		{
			continue;
		}

		FBeamNoisePayload Payload{};

		std::memcpy(
		    &Payload,
		    reinterpret_cast<const uint8*>(Particle) +
		        PayloadOffset,
		    sizeof(Payload));

		AdvanceNoise(Payload, DeltaTime, UpdateInterval);

		std::memcpy(
		    reinterpret_cast<uint8*>(Particle) + PayloadOffset,
		    &Payload,
		    sizeof(Payload));
	}
}
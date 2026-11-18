#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleSubUVMovie.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleRange.h"

namespace
{
	FParticleSubUVPayload& PayloadOf(FBaseParticle& Particle, int32 PayloadOffset)
	{
		return *reinterpret_cast<FParticleSubUVPayload*>(reinterpret_cast<uint8*>(&Particle) + PayloadOffset);
	}
}

void UParticleModuleSubUVMovie::Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle)
{
	const int32 Total = GetTotalFrames(Owner);
	const float Start = bRandomStartFrame
		? FParticleFloatRange(0.0f, static_cast<float>(Total)).Sample()
		: StartFrame;

	FParticleSubUVPayload& Payload = PayloadOf(Particle, PayloadOffset);
	Payload.StartOffset = std::floor(Start);
	Payload.Frame = std::clamp(Payload.StartOffset, 0.0f, static_cast<float>(Total - 1));
}

void UParticleModuleSubUVMovie::Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime)
{
	const int32 Total = GetTotalFrames(Owner);
	const float TotalF = static_cast<float>(Total);

	for (int32 i = 0; i < Owner.ActiveParticles; ++i)
	{
		DECLARE_PARTICLE_PTR(Particle, Owner.ParticleData + Owner.ParticleStride * Owner.ParticleIndices[i]);
		if (Particle->Flags & STATE_Particle_Freeze) continue;
		if (Particle->OneOverMaxLifetime <= 0.0f) continue;

		// 태어난 뒤 흐른 시간(초). RelativeTime은 수명에 대한 비율이라 수명으로 되돌린다.
		const float Age = Particle->RelativeTime / Particle->OneOverMaxLifetime;

		FParticleSubUVPayload& Payload = PayloadOf(*Particle, PayloadOffset);
		float Frame = Payload.StartOffset + Age * FrameRate;
		if (bLoop)
		{
			Frame = std::fmod(Frame, TotalF);
			if (Frame < 0.0f) Frame += TotalF;
		}
		else
		{
			Frame = std::clamp(Frame, 0.0f, TotalF - 1.0f);
		}
		Payload.Frame = std::clamp(std::floor(Frame), 0.0f, TotalF - 1.0f);
	}
}

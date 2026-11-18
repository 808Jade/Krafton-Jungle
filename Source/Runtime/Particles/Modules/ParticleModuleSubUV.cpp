#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleSubUV.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleHelper.h"

namespace
{
	FParticleSubUVPayload& PayloadOf(FBaseParticle& Particle, int32 PayloadOffset)
	{
		return *reinterpret_cast<FParticleSubUVPayload*>(reinterpret_cast<uint8*>(&Particle) + PayloadOffset);
	}
}

void UParticleModuleSubUV::Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle)
{
	const int32 Total = GetTotalFrames(Owner);
	const float Start = std::clamp(std::floor(StartFrame), 0.0f, static_cast<float>(Total - 1));
	PayloadOf(Particle, PayloadOffset).Frame = Start;
}

void UParticleModuleSubUV::Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime)
{
	const int32 Total = GetTotalFrames(Owner);
	const int32 Last = Total - 1;
	const int32 Start = std::clamp(static_cast<int32>(std::floor(StartFrame)), 0, Last);
	const int32 End = EndFrame < 0.0f ? Last : std::clamp(static_cast<int32>(std::floor(EndFrame)), 0, Last);
	const int32 Direction = End >= Start ? 1 : -1;
	const int32 Count = std::abs(End - Start) + 1;

	for (int32 i = 0; i < Owner.ActiveParticles; ++i)
	{
		DECLARE_PARTICLE_PTR(Particle, Owner.ParticleData + Owner.ParticleStride * Owner.ParticleIndices[i]);
		if (Particle->Flags & STATE_Particle_Freeze) continue;

		// 수명을 Count등분해서 각 프레임이 같은 시간씩 보이게 한다.
		const float T = std::clamp(Particle->RelativeTime, 0.0f, 1.0f);
		const int32 Step = std::min(static_cast<int32>(T * static_cast<float>(Count)), Count - 1);
		PayloadOf(*Particle, PayloadOffset).Frame = static_cast<float>(Start + Direction * Step);
	}
}

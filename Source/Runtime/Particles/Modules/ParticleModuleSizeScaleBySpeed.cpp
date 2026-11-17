#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleSizeScaleBySpeed.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleHelper.h"

namespace
{
	// 속도에 따른 한 축의 배율. 음수 크기가 되지 않게 0으로 막고 상한을 넘지 않게 한다.
	float AxisFactor(float SpeedScale, float MaxScale, float Speed)
	{
		return std::clamp(1.0f + SpeedScale * Speed, 0.0f, std::max(MaxScale, 0.0f));
	}
}

void UParticleModuleSizeScaleBySpeed::Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime)
{
	for (int32 i = 0; i < Owner.ActiveParticles; ++i)
	{
		DECLARE_PARTICLE_PTR(Particle, Owner.ParticleData + Owner.ParticleStride * Owner.ParticleIndices[i]);
		if (Particle->Flags & STATE_Particle_Freeze) continue;

		const float Speed = Particle->Velocity.Length();
		Particle->Size.X *= AxisFactor(SpeedScale.X, MaxScale.X, Speed);
		Particle->Size.Y *= AxisFactor(SpeedScale.Y, MaxScale.Y, Speed);
		Particle->Size.Z *= AxisFactor(SpeedScale.Z, MaxScale.Z, Speed);

		// 속도에 비례하는 절대 길이. 기본 크기가 작아도 같은 속도면 같은 길이만큼 늘어난다.
		if (LengthPerSpeed != 0.0f)
		{
			Particle->Size.Y = std::max(Particle->Size.Y + LengthPerSpeed * Speed, 0.0f);
		}
		if (MaxLength > 0.0f)
		{
			Particle->Size.Y = std::min(Particle->Size.Y, MaxLength);
		}
	}
}

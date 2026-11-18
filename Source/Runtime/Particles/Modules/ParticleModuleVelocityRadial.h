#pragma once

#include "Particles/ParticleModule.h"
#include "Particles/ParticleRange.h"

// 파티클마다 무작위 방향으로 Speed만큼의 속도를 준다. 방향은 구 표면에서 고르게 뽑는다. (불꽃놀이, 폭발, 충격파)
// Initial Velocity는 박스 안에서 뽑아서 모서리 쪽이 더 빠른 네모난 분포가 되지만, 이 모듈은 속도가 방향과 상관없이 Speed 범위 안이라 둥글게 퍼진다.
//
// AxisScale로 방향을 축마다 눌러 납작하게 만들 수 있다. 예) (1, 0.04, 1)이면 XZ 평면에 놓인 얇은 고리, (1, 1, 0)이면 수평 원반이다.
// 기존 속도에 더한다. (Initial Velocity와 함께 써도 된다)
class UParticleModuleVelocityRadial : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleVelocityRadial, UParticleModule)
	REFLECT_START(UParticleModuleVelocityRadial)
		PROPERTY(Speed)
		PROPERTY(AxisScale)
	REFLECT_END()

public:
	UParticleModuleVelocityRadial() = default;

	void Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle) override;
	bool NeedsSpawn() const override { return true; }

	// 속도 크기 범위(m/s)
	FParticleFloatRange Speed = FParticleFloatRange(5.0f, 8.0f);

	// 단위 방향에 축마다 곱하는 값. 곱한 뒤 다시 정규화하지 않으므로 값이 작은 축은 그만큼 속도가 줄어든다.
	FVector AxisScale = FVector(1.0f, 1.0f, 1.0f);
};

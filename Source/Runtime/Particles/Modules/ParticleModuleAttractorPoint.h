#pragma once

#include "Particles/ParticleModule.h"

// 한 점 쪽으로 파티클을 끌어당기는 가속도를 매 틱 더하는 모듈. 소용돌이, 블랙홀, 자석 같은 효과에 쓴다.
// - 점에서 Range 안에 있는 파티클만 영향을 받는다.
// - 속도는 매 틱 BaseVelocity에서 다시 시작하므로(ResetParticleParameters) Velocity와 BaseVelocity에 함께 누적한다.
// - Acceleration과 같은 단계(순서 0)에서 돈다. 충돌(1000)보다 앞이라 끌려간 결과로 충돌을 판정한다.
class UParticleModuleAttractorPoint : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleAttractorPoint, UParticleModule)
	REFLECT_START(UParticleModuleAttractorPoint)
		PROPERTY(Position)
		PROPERTY(bUseWorldSpacePosition)
		PROPERTY(Range)
		PROPERTY(Strength)
		PROPERTY(bStrengthByDistance)
	REFLECT_END()

public:
	UParticleModuleAttractorPoint() = default;

	void Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime) override;
	bool NeedsUpdate() const override { return true; }

	// 끌어당기는 점의 위치(m). 기본은 이미터 기준이라 컴포넌트를 옮기고 돌리면 같이 움직인다.
	FVector Position = FVector(0.0f, 0.0f, 1.0f);

	// true이면 Position을 월드 좌표로 본다.
	bool bUseWorldSpacePosition = false;

	// 점에서 이 거리(m) 안의 파티클만 끌어당긴다. 0 이하이면 아무 영향이 없다.
	float Range = 3.0f;

	// 가속도 크기(m/s^2). 음수이면 점에서 밀어낸다.
	float Strength = 10.0f;

	// true이면 점에 가까울수록 세게 끌어당긴다. (점에서 Strength, Range 경계에서 0으로 선형 감소)
	// false이면 Range 안에서 거리와 상관없이 Strength로 끌어당긴다.
	bool bStrengthByDistance = true;
};

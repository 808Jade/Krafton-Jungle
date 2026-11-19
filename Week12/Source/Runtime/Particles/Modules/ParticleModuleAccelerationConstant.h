#pragma once

#include "Particles/ParticleModule.h"

// 살아 있는 파티클에 일정한 가속도를 매 틱 더하는 모듈. 중력은 (0, 0, -9.81)로 표현한다.
// 속도는 매 틱 BaseVelocity에서 다시 시작하므로(ResetParticleParameters),
// 가속은 Velocity와 BaseVelocity에 함께 누적해야 다음 틱에 사라지지 않는다.
class UParticleModuleAccelerationConstant : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleAccelerationConstant, UParticleModule)
	REFLECT_START(UParticleModuleAccelerationConstant)
		PROPERTY(Acceleration)
		PROPERTY(bAlwaysInWorldSpace)
	REFLECT_END()

public:
	UParticleModuleAccelerationConstant() = default;

	void Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime) override;
	bool NeedsUpdate() const override { return true; }

	// 가속도(m/s^2). 기본값은 월드 -Z 방향 중력이다.
	FVector Acceleration = FVector(0.0f, 0.0f, -9.81f);

	// true이면 Acceleration을 월드 축 기준으로 본다. 로컬 스페이스 이미터가 회전해도 중력은 아래를 향한다.
	// false이면 시뮬레이션 공간 기준이다. 월드 스페이스 이미터에서는 같은 결과다.
	bool bAlwaysInWorldSpace = false;
};

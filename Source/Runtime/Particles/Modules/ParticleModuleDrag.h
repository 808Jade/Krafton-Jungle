#pragma once

#include "Particles/ParticleModule.h"

// 속도에 비례하는 저항으로 파티클을 감속시킨다. 빠를수록 많이 줄어서 처음엔 휙 날아가다 금방 느려진다. (불꽃놀이 별, 폭발 파편, 연기)
// 매 틱 속도에 exp(-DragCoefficient * dt)를 곱하므로 프레임 속도와 상관없이 같은 결과가 나온다.
// 예) 계수 2이면 0.5초 뒤 속도가 약 37%, 1초 뒤 약 14%로 줄어든다.
// Velocity와 BaseVelocity에 함께 곱해야 다음 틱의 속도 리셋에 지워지지 않는다. (가속 모듈과 같은 이유)
class UParticleModuleDrag : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleDrag, UParticleModule)
	REFLECT_START(UParticleModuleDrag)
		PROPERTY(DragCoefficient)
	REFLECT_END()

public:
	UParticleModuleDrag() = default;

	void Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime) override;
	bool NeedsUpdate() const override { return true; }
	// 가속(0)을 반영한 속도에 저항을 건다. 충돌(1000)보다는 앞이다.
	int32 GetUpdateOrder() const override { return 10; }

	// 저항 계수(1/s). 0이면 아무 영향이 없다.
	float DragCoefficient = 1.0f;
};

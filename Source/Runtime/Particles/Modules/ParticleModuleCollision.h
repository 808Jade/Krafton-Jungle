#pragma once

#include "Particles/ParticleModule.h"

// 파티클이 이번 틱에 움직일 구간(Location → Location + Velocity * dt)을 월드의 스태틱 메시와 검사해서,
// 맞으면 법선으로 속도를 반사한다. 파티클마다 월드 선분 트레이스를 한 번 쏘는 CPU 방식이다.
//
// - 반사한 속도에 DampingFactor를 곱한다. Velocity와 BaseVelocity를 함께 반사해야 다음 틱의 리셋에 지워지지 않는다.
// - 충돌 횟수는 파티클별 payload에 센다. MaxCollisions번째 충돌에서 CollisionCompletionOption을 적용한다.
// - 가속·속도 모듈보다 항상 뒤에 돈다(GetUpdateOrder). 이번 틱의 가속이 반영된 속도로 검사하기 위해서다.
// - 월드에 등록된 스태틱 메시만 대상이다. 빌보드·에디터 아이콘, 자기 자신의 액터는 무시한다.
class UParticleModuleCollision : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleCollision, UParticleModule)
	REFLECT_START(UParticleModuleCollision)
		PROPERTY(DampingFactor)
		PROPERTY(MaxCollisions)
		PROPERTY(CollisionCompletionOption)
	REFLECT_END()

public:
	UParticleModuleCollision() = default;

	int32 GetRequiredPayloadBytes() const override { return static_cast<int32>(sizeof(int32)); }

	void Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime) override;
	bool NeedsUpdate() const override { return true; }
	int32 GetUpdateOrder() const override { return 1000; }

	// 반사한 속도에 곱하는 값. 1이면 에너지를 잃지 않고, 0이면 충돌 지점에서 속도가 사라진다.
	float DampingFactor = 0.5f;

	// 이 횟수만큼 충돌하면 CollisionCompletionOption을 적용한다. 1이면 첫 충돌이 마지막이다.
	int32 MaxCollisions = 3;

	EParticleCollisionComplete CollisionCompletionOption = EParticleCollisionComplete::Kill;
};

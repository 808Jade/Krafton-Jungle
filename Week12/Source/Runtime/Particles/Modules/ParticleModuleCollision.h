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
		PROPERTY(ParticleRadiusScale)
		PROPERTY(MaxCollisions)
		PROPERTY(CollisionCompletionOption)
		PROPERTY(bGenerateEvents)
		PROPERTY(EventName)
		PROPERTY(MaxEventsPerFrame)
	REFLECT_END()

public:
	UParticleModuleCollision() = default;

	int32 GetRequiredPayloadBytes() const override { return static_cast<int32>(sizeof(int32)); }

	void Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime) override;
	bool NeedsUpdate() const override { return true; }
	int32 GetUpdateOrder() const override { return 1000; }

	// 반사한 속도에 곱하는 값. 1이면 에너지를 잃지 않고, 0이면 충돌 지점에서 속도가 사라진다.
	float DampingFactor = 0.5f;

	// 파티클 크기의 절반(반경)에 곱하는 값. 0이면 크기 없는 점으로 검사한다.
	// 1이면 파티클 가장자리가 표면에 닿을 때 충돌한다. 스프라이트는 Size X·Y 중 큰 값, 메시는 메시 AABB의 가장 큰 변 x Size를 기준으로 한다.
	// 이동 방향의 선분 하나로 검사하는 근사라서, 중심선은 표면 위인데 가장자리만 모서리 밖에 걸친 경우는 잡지 못한다.
	float ParticleRadiusScale = 1.0f;

	// 이 횟수만큼 충돌하면 CollisionCompletionOption을 적용한다. 1이면 첫 충돌이 마지막이다.
	int32 MaxCollisions = 3;

	EParticleCollisionComplete CollisionCompletionOption = EParticleCollisionComplete::Kill;

	// true이면 충돌할 때마다 충돌 이벤트를 만든다. (마지막 충돌 포함)
	// 이벤트는 UParticleSystemComponent::OnParticleCollide(스크립트 포함)와, 같은 시스템의 Event Receiver 모듈로 전달된다.
	// 받는 쪽이 없으면 만들지 않는다. 한 프레임에 모으는 수에는 상한이 있다.
	bool bGenerateEvents = false;

	// 이벤트에 실어 보내는 이름. 이미터마다 다르게 주면 받는 쪽에서 어느 충돌인지 구분할 수 있다.
	FString EventName = "ParticleCollision";

	// 이 모듈이 한 프레임에 만드는 이벤트의 최대 개수. 파티클이 많을 때 이벤트가 쏟아져 느려지는 것을 막는다.
	// 넘으면 그 프레임의 나머지 이벤트는 버려진다. (Event Receiver의 스폰도 그만큼 빠진다) 0 이하이면 제한이 없다.
	int32 MaxEventsPerFrame = 128;
};

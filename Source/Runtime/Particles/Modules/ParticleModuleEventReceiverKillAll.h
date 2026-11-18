#pragma once

#include "Particles/ParticleModule.h"

// 같은 파티클 시스템의 충돌 이벤트를 받으면, 이 이미터의 살아 있는 파티클을 모두 지운다.
// 예) 한 번 충돌하면 연기 이미터를 전부 걷어낸다.
// 이벤트를 만든 다음 프레임에 반응한다. 이 모듈이 있는 이미터의 스폰 자체는 막지 않는다.
class UParticleModuleEventReceiverKillAll : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleEventReceiverKillAll, UParticleModule)
	REFLECT_START(UParticleModuleEventReceiverKillAll)
		PROPERTY(EventName)
	REFLECT_END()

public:
	UParticleModuleEventReceiverKillAll() = default;

	void Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime) override;
	bool NeedsUpdate() const override { return true; }
	bool ReceivesParticleEvents() const override { return true; }

	// 반응할 이벤트 이름. 비워 두면 모든 이벤트에 반응한다.
	FString EventName = "ParticleCollision";
};

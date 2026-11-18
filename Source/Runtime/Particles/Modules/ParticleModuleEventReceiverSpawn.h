#pragma once

#include "Particles/ParticleModule.h"
#include "Particles/ParticleRange.h"

// 같은 파티클 시스템의 다른 이미터(예: Collision 모듈)가 만든 충돌 이벤트를 받아, 이 이미터에서 파티클을 스폰한다.
// 예) 불똥이 바닥에 닿은 자리에서 연기나 새 불똥을 터뜨린다.
//
// - 이벤트를 만든 다음 프레임에 반응한다. (이미터 갱신 순서와 상관없이 결과가 같도록)
// - 이 이미터는 스폰 비율이 0이고 Required의 EmitterLoops가 0(무한)이어야 계속 이벤트를 받을 수 있다.
// - 동시에 살아 있을 수 있는 파티클 수(슬롯)는 Required의 MaxActiveParticles로 정해진다. (이벤트로 몇 개가 필요한지는 알 수 없어서)
// - 스폰한 파티클의 흩어짐은 이 이미터의 Initial Velocity 같은 Spawn 모듈이 더한다.
// - 같은 이미터가 같은 이름의 이벤트를 만들면서 받으면 스폰이 꼬리를 물고 늘어난다. 이름을 다르게 주자.
class UParticleModuleEventReceiverSpawn : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleEventReceiverSpawn, UParticleModule)
	REFLECT_START(UParticleModuleEventReceiverSpawn)
		PROPERTY(EventName)
		PROPERTY(SpawnCount)
		PROPERTY(bSpawnAtEventLocation)
		PROPERTY(NormalSpeed)
		PROPERTY(SurfaceOffset)
	REFLECT_END()

public:
	UParticleModuleEventReceiverSpawn() = default;

	void Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime) override;
	bool NeedsUpdate() const override { return true; }
	bool ReceivesParticleEvents() const override { return true; }

	// 반응할 이벤트 이름. Collision 모듈의 EventName과 같아야 한다. 비워 두면 모든 이벤트에 반응한다.
	FString EventName = "ParticleCollision";

	// 이벤트 하나당 스폰하는 파티클 수
	int32 SpawnCount = 5;

	// true이면 이벤트의 충돌점에서, false이면 이 이미터의 원점에서 스폰한다.
	bool bSpawnAtEventLocation = true;

	// 충돌 법선 방향으로 주는 초기 속도(m/s) 범위. 표면에서 튀어나가는 모양을 만든다. 0이면 속도를 주지 않는다.
	FParticleFloatRange NormalSpeed;

	// 표면 안쪽에서 태어나지 않게, 충돌점에서 법선 방향으로 띄우는 거리(m)
	float SurfaceOffset = 0.01f;
};

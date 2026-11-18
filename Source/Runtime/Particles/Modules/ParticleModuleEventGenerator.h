#pragma once

#include "Particles/ParticleModule.h"

// 파티클의 수명에 맞춰 이벤트를 만든다. Collision 모듈이 "부딪힐 때" 이벤트를 만든다면, 이 모듈은 "죽을 때"와 "살아 있는 동안 일정 간격으로" 만든다.
// 같은 시스템의 Event Receiver Spawn 모듈이 받아서 다른 이미터에서 파티클을 터뜨린다.
//
// - Death: 파티클이 죽기 직전 틱에 한 번 만든다. 불꽃놀이 로켓이 정점에서 터지고, 터진 별이 사라질 때 잔불꽃이 튀는 연쇄를 만든다.
//   수신 쪽이 한 프레임 늦게 반응하므로 파티클이 사라지는 프레임에 맞춰 새 파티클이 나온다.
// - Trail: 살아 있는 동안 TrailRate(초당 개수)로 현재 위치에서 만든다. 날아가는 파티클 뒤에 꼬리 불꽃을 남기는 데 쓴다.
//
// 이벤트의 Location은 파티클 위치, Velocity는 파티클 속도, Normal은 속도 방향의 단위 벡터다. (Receiver의 NormalSpeed를 음수로 주면 날아온 반대쪽으로 흩어진다)
// 받는 쪽(Event Receiver 모듈 또는 델리게이트 구독자)이 없으면 만들지 않는다.
class UParticleModuleEventGenerator : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleEventGenerator, UParticleModule)
	REFLECT_START(UParticleModuleEventGenerator)
		PROPERTY(DeathEventName)
		PROPERTY(DeathEventChance)
		PROPERTY(TrailEventName)
		PROPERTY(TrailRate)
		PROPERTY(MaxEventsPerFrame)
	REFLECT_END()

public:
	UParticleModuleEventGenerator() = default;

	// 파티클마다 죽음 이벤트를 이미 보냈는지, 꼬리 이벤트까지 남은 시간을 센다.
	int32 GetRequiredPayloadBytes() const override { return 2 * static_cast<int32>(sizeof(float)); }

	void Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle) override;
	void Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime) override;
	bool NeedsSpawn() const override { return true; }
	bool NeedsUpdate() const override { return true; }
	// 이번 틱의 이동·가속·충돌이 끝난 최종 위치와 속도로 이벤트를 만든다.
	int32 GetUpdateOrder() const override { return 2000; }

	// 죽음 이벤트 이름. 비워 두면 만들지 않는다.
	FString DeathEventName = "ParticleDeath";

	// 죽을 때 이벤트를 만들 확률(0~1). 별이 수백 개일 때 일부만 반응하게 해서 이벤트 수를 줄인다.
	float DeathEventChance = 1.0f;

	// 꼬리 이벤트 이름. 비워 두면 만들지 않는다.
	FString TrailEventName;

	// 파티클 하나가 초당 만드는 꼬리 이벤트 수. 한 틱에는 최대 하나만 만든다.
	float TrailRate = 30.0f;

	// 이 모듈이 한 프레임에 만드는 이벤트의 최대 개수. 넘으면 그 프레임의 나머지는 버려진다. 0 이하이면 제한이 없다.
	int32 MaxEventsPerFrame = 64;
};

#pragma once

#include "PrimitiveComponent.h"
#include "Core/TDelegate.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleSystem.h"
#include "ParticleSystemComponent.generated.h"
class UPointLightComponent;
 UCLASS()
class UParticleSystemComponent : public UPrimitiveComponent
{
	GENERATED_BODY()
public:
	UParticleSystemComponent();
	~UParticleSystemComponent();
	virtual void TickComponent(float DeltaTime) override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;
	FBox CalcBounds() const override;
	virtual void SetActive(bool bNewActive) override;
	void InitParticles();
	void ResetParticles();
	void SetTemplate(UParticleSystem* InTemplate);
	void ActivateSystem(bool bReset = false);
	void DeactivateSystem();
	void ResetSystem();
	bool HasCompleted() const;
	void PostApplyProperties(const FProperty* ChangedProperty = nullptr) override;
	void SetLODLevel(int32 InLODLevel);
	int32 GetLODLevel() const { return LODLevel; }
	int32 DetermineLODLevelForLocation(const FVector& EffectLocation);
	EParticleSystemLODMethod GetCurrentLODMethod() const;

	// 파티클이 다른 컴포넌트와 충돌했을 때 호출된다. Collision 모듈의 bGenerateEvents를 켜야 만들어진다.
	// 한 프레임의 모든 이미터 갱신이 끝난 뒤에 모아서 호출하므로, 콜백 안에서 이 컴포넌트나 파티클을 건드려도 안전하다.
	TDelegate<const FParticleCollisionEvent&> OnParticleCollide;

	// Collision 모듈이 부르는 함수. 이 프레임의 이벤트 목록에 추가한다.
	void QueueCollisionEvent(FParticleCollisionEvent&& Event);

	// 충돌 이벤트를 만들 가치가 있는지. 델리게이트 구독자나 이 시스템의 Event Receiver 모듈이 있을 때 true다.
	bool WantsCollisionEvents() const { return bHasEventReceivers || OnParticleCollide.IsBound(); }

	// 직전 프레임에 만들어진 충돌 이벤트. Event Receiver 모듈이 이번 프레임 갱신에서 읽는다. (한 프레임 지나면 사라진다)
	// 이벤트가 만들어진 다음 프레임에 반응하므로 이미터 갱신 순서와 상관없이 같은 결과가 나온다.
	const TArray<FParticleCollisionEvent>& GetReceivedCollisionEvents() const { return ReceivedCollisionEvents; }

	// 한 컴포넌트가 동시에 켤 수 있는 파티클 라이트(Light 모듈)의 최대 개수. 라이트 하나하나가 렌더 비용이라 상한을 둔다.
	static constexpr int32 MaxParticleLights = 64;

	// Light 모듈이 부르는 함수. 이번 프레임에 쓸 포인트 라이트를 풀에서 하나 빌려 준다. (켜 둔 상태로 돌려주고, 위치·색·세기는 부른 쪽이 정한다)
	// 풀이 가득 차면 nullptr이다. 이번 프레임에 빌려 가지 않은 라이트는 프레임 끝에 꺼진다.
	// 풀의 라이트는 이 컴포넌트가 소유하는 런타임 전용 컴포넌트라 액터의 컴포넌트 목록(씬 저장)에 들어가지 않고, 그림자도 만들지 않는다.
	UPointLightComponent* AcquireParticleLight();

protected:
	void OnUnregister() override;

private:
	void BeginParticleLights();
	void EndParticleLights();
	void DestroyParticleLights();
	TArray<UPointLightComponent*> ParticleLights;
	int32 ParticleLightsUsed = 0;

	void FlushCollisionEvents();
	void RefreshEventReceivers();
	// 이번 프레임에 쌓이는 이벤트와, 직전 프레임에 만들어져 Event Receiver가 읽는 이벤트
	TArray<FParticleCollisionEvent> PendingCollisionEvents;
	TArray<FParticleCollisionEvent> ReceivedCollisionEvents;
	bool bHasEventReceivers = false;

	bool GetEmitterBoundingBox(FBox& OutBoundingBox) const;
	int32 ClampLODLevel(int32 InLODLevel) const;

	void InitializeMeshInstancingTest();

public:
	TArray<FParticleEmitterInstance*> EmitterInstances;
	UPROPERTY(EditAnywhere)
	UParticleSystem* Template = nullptr;

	UPROPERTY(EditAnywhere)
	float CustomTimeDilation = 1.0f;

	int32	LODLevel = 0;
	float	AccumLODDistanceCheckTime = 0.0f;
	bool	bOverrideLODMethod = false;
	EParticleSystemLODMethod LODMethod = EParticleSystemLODMethod::Automatic;

	// 테스트/디버깅 용
	bool	bMeshInstancingTest = true;

};
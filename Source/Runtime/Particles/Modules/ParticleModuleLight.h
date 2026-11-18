#pragma once

#include "Particles/ParticleModule.h"
#include "Particles/ParticleCurve.h"

// 파티클이 있는 자리에 실제 포인트 라이트를 켠다. (UE의 Light 모듈) 불똥이 바닥과 벽을 비추고, 폭발이 주변을 번쩍 밝힌다.
//
// - 라이트는 파티클마다 만들지 않고 컴포넌트가 가진 라이트 풀에서 매 프레임 빌려 쓴다. 쓰지 않은 라이트는 꺼진다.
//   씬에는 저장되지 않는 런타임 전용 라이트이다. 그림자는 기본으로 만들지 않고, bCastShadows를 켠 모듈의 라이트만 만든다.
// - 라이트 하나는 비용이 있다. 파티클이 수백 개여도 SpawnFraction과 MaxLights로 일부에만 켜는 것이 보통이다.
//   한 컴포넌트가 동시에 켤 수 있는 파티클 라이트에는 상한이 있고(UParticleSystemComponent::MaxParticleLights),
//   엔진 전체의 로컬 라이트 상한도 함께 쓴다.
// - 모든 이동·충돌 모듈이 끝난 뒤(순서 3000)의 최종 위치를 따라간다.
// - 파티클은 스폰할 때 모든 값이 0으로 시작하므로 Color·Size 모듈이 있어야 라이트의 색과 반경이 생긴다.
//   (색이 검정이거나 크기가 0이면 그 파티클의 라이트는 켜지 않는다) 태어난 다음 프레임부터 켜진다.
// - 라이트를 켤 파티클은 태어날 때 SpawnFraction 확률로 정한다. MaxLights보다 많이 걸리면 파티클 배열 앞쪽부터 켠다.
class UParticleModuleLight : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleLight, UParticleModule)
	REFLECT_START(UParticleModuleLight)
		PROPERTY(bUseParticleColor)
		PROPERTY(TintColor)
		PROPERTY(Intensity)
		PROPERTY(bScaleByParticleAlpha)
		PROPERTY(bUseBrightnessOverLife)
		PROPERTY(RadiusScale)
		PROPERTY(MaxRadius)
		PROPERTY(FalloffExponent)
		PROPERTY(SpawnFraction)
		PROPERTY(MaxLights)
		PROPERTY(bCastShadows)
		PROPERTY(ShadowMapResolution)
		PROPERTY(ShadowMinIntensity)
	REFLECT_END()

public:
	UParticleModuleLight() = default;

	// 이 파티클이 라이트를 켤 대상인지(1) 아닌지(0)
	int32 GetRequiredPayloadBytes() const override { return static_cast<int32>(sizeof(float)); }

	void Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle) override;
	void Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime) override;
	bool NeedsSpawn() const override { return true; }
	bool NeedsUpdate() const override { return true; }
	int32 GetUpdateOrder() const override { return 3000; }

	void GetCurves(TArray<FParticleCurveRef>& OutCurves) override;
	using Super::Serialize;
	void Serialize(FStructuredArchive::FRecord Record) override;

	// true이면 라이트 색 = 파티클의 현재 색(색 모듈 결과) x TintColor. false이면 TintColor만 쓴다.
	bool bUseParticleColor = true;
	FLinearColor TintColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	// 라이트 세기의 기준값. 색이 1을 넘으면(HDR) 넘은 만큼 세기에 더한다.
	float Intensity = 3.0f;

	// true이면 세기에 파티클의 알파를 곱한다. 파티클이 사라질 때 라이트도 같이 어두워진다.
	bool bScaleByParticleAlpha = true;

	// true이면 세기에 수명(0~1) 커브를 곱한다. 커브는 커브 에디터에서 편집한다.
	bool bUseBrightnessOverLife = false;
	FInterpCurveFloat BrightnessOverLife = FInterpCurveFloat::MakeLinear(1.0f, 1.0f);

	// 영향 반경(m) = RadiusScale x 파티클 크기(X·Y 중 큰 값) x 컴포넌트 스케일. 크기가 줄면 반경도 줄어든다.
	float RadiusScale = 12.0f;
	// 반경의 상한(m)
	float MaxRadius = 8.0f;

	// 거리에 따른 감쇠 지수. 클수록 가까운 곳만 밝다.
	float FalloffExponent = 1.0f;

	// 파티클 중 라이트를 켤 비율(0~1). 태어날 때 한 번 정한다.
	float SpawnFraction = 1.0f;

	// 이 모듈이 한 프레임에 켜는 라이트의 최대 개수. 0 이하이면 컴포넌트 상한까지 쓴다.
	int32 MaxLights = 8;

	// true이면 이 모듈이 켜는 라이트가 그림자를 만든다. 점광원 그림자는 6방향으로 그림자 맵을 그려서 비싸므로
	// 번개 섬광처럼 크고 드물게 켜지는 라이트에만 쓴다. (불똥처럼 많고 작은 라이트에는 쓰지 않는다)
	bool bCastShadows = false;
	// 그림자 맵 한 면의 해상도. 128~4096으로 제한된다.
	int32 ShadowMapResolution = 512;
	// 라이트 세기가 이 값 이상일 때만 그림자를 그린다. 희미하게 꺼져 가는 동안은 그림자가 잘 안 보이니 비용을 아낀다. 0이면 항상.
	float ShadowMinIntensity = 0.0f;
};

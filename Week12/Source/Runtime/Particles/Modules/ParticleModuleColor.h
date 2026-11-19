#pragma once

#include "Particles/ParticleModule.h"
#include "Particles/ParticleRange.h"
#include "Particles/ParticleCurve.h"

class UParticleModuleColor : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleColor, UParticleModule)
	REFLECT_START(UParticleModuleColor)
		PROPERTY(StartColor)
		PROPERTY(bUseColorOverLife)
	REFLECT_END()

public:
	UParticleModuleColor() = default;

	void Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle) override;

	void Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime) override;

	bool NeedsSpawn() const override { return true; }
	bool NeedsUpdate() const override { return bUseColorOverLife; }

	void GetCurves(TArray<FParticleCurveRef>& OutCurves) override;

	// 커브는 TArray라 PROPERTY로 등록할 수 없다. 리플렉션 프로퍼티는 Super가 저장하고, 커브는 여기서 직접 저장한다.
	// 예전 파일(EndColor)을 읽으면 같은 결과가 나오도록 커브로 옮긴다.
	using Super::Serialize;
	void Serialize(FStructuredArchive::FRecord Record) override;

	// 수명 끝에서 투명해지는 페이드 아웃 커브(알파 1→0)를 만든다.
	void SetFadeOut();

	FParticleColorRange StartColor;

	// true이면 수명(0~1)에 따라 스폰 때 정한 색에 채널별 배율 커브를 곱한다. 커브는 커브 에디터에서 편집한다.
	// 기본값은 예전 기본값(EndColor = 흰색, 알파 0)처럼 색은 그대로 두고 알파만 1에서 0으로 줄인다.
	bool bUseColorOverLife = false;
	FInterpCurveFloat ColorScaleR = FInterpCurveFloat::MakeLinear(1.0f, 1.0f);
	FInterpCurveFloat ColorScaleG = FInterpCurveFloat::MakeLinear(1.0f, 1.0f);
	FInterpCurveFloat ColorScaleB = FInterpCurveFloat::MakeLinear(1.0f, 1.0f);
	FInterpCurveFloat AlphaScale = FInterpCurveFloat::MakeLinear(1.0f, 0.0f);
};

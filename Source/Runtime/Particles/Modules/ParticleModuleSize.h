#pragma once

#include "Particles/ParticleModule.h"
#include "Particles/ParticleRange.h"
#include "Particles/ParticleCurve.h"

class UParticleModuleSize : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleSize, UParticleModule)
	REFLECT_START(UParticleModuleSize)
		PROPERTY(StartSize)
		PROPERTY(bUseSizeOverLife)
	REFLECT_END()

public:
	UParticleModuleSize() = default;

	void Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle) override;

	void Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime) override;

	bool NeedsSpawn() const override { return true; }
	bool NeedsUpdate() const override { return bUseSizeOverLife; }

	void GetCurves(TArray<FParticleCurveRef>& OutCurves) override;

	// 커브는 TArray라 PROPERTY로 등록할 수 없다. 리플렉션 프로퍼티는 Super가 저장하고, 커브는 여기서 직접 저장한다.
	// 예전 파일(EndSize)을 읽으면 같은 결과가 나오도록 커브로 옮긴다.
	using Super::Serialize;
	void Serialize(FStructuredArchive::FRecord Record) override;

	FParticleVectorRange StartSize = FParticleVectorRange(FVector(1.0f, 1.0f, 1.0f));

	// true이면 수명(0~1)에 따라 스폰 때 정한 크기에 축별 배율 커브를 곱한다. 커브는 커브 에디터에서 편집한다.
	bool bUseSizeOverLife = false;
	FInterpCurveFloat SizeScaleX = FInterpCurveFloat::MakeLinear(1.0f, 1.0f);
	FInterpCurveFloat SizeScaleY = FInterpCurveFloat::MakeLinear(1.0f, 1.0f);
	FInterpCurveFloat SizeScaleZ = FInterpCurveFloat::MakeLinear(1.0f, 1.0f);
};

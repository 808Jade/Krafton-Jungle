#pragma once

#include "Particles/ParticleModule.h"
#include "Particles/ParticleBeamData.h"
#include "ParticleModuleBeamNoise.generated.h"

UCLASS()
class UParticleModuleBeamNoise : public UParticleModule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	float Amplitude = 0.5f;

	// Noise 중심선의 구간 수.
	// 점 개수는 NoiseSegmentCount + 1.
	UPROPERTY(EditAnywhere)
	int32 NoiseSegmentCount = 8;

	// 0이면 생성 시 모양을 계속 유지.
	UPROPERTY(EditAnywhere)
	float UpdateInterval = 0.0f;

	UPROPERTY(EditAnywhere)
	int32 RandomSeed = 12345;

	UPROPERTY(EditAnywhere)
	bool bSmoothNoise = false;

	int32 GetRequiredPayloadBytes() const override
	{
		return static_cast<int32>(sizeof(FBeamNoisePayload));
	}

	bool NeedsSpawn() const override { return true; }

	// B단계에서 UpdateInterval 변경도 반영할 수 있도록
	// 처음부터 Update 목록에 등록한다.
	bool NeedsUpdate() const override { return true; }

	void Spawn(
	    FParticleEmitterInstance& Owner,
	    int32 PayloadOffset,
	    float SpawnTime,
	    FBaseParticle& Particle) override;

	void Update(
	    FParticleEmitterInstance& Owner,
	    int32 PayloadOffset,
	    float DeltaTime) override;
};
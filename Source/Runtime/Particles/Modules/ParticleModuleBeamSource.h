#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleBeamSource.generated.h"

UCLASS()
class UParticleModuleBeamSource : public UParticleModule
{
	GENERATED_BODY()

public:
	// 컴포넌트 로컬 좌표.
	UPROPERTY(EditAnywhere)
	FVector SourcePosition = FVector(0.0f, 0.0f, 0.0f);

	// true: 생성 시에만 평가.
	// false: 매 프레임 다시 평가.
	UPROPERTY(EditAnywhere)
	bool bLockOnSpawn = true;
};

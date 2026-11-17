#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleBeamTarget.generated.h"

UCLASS()
class UParticleModuleBeamTarget : public UParticleModule
{
	GENERATED_BODY()

public:
	// 컴포넌트 로컬 좌표.
	UPROPERTY(EditAnywhere)
	FVector TargetPosition = FVector(5.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere)
	bool bLockOnSpawn = true;
};

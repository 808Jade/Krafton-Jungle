#pragma once

#include "Particles/ParticleModule.h"
#include "Particles/ParticleBeamData.h"
#include "Particles/ParticleRange.h"
#include "ParticleModuleBeamTarget.generated.h"

UCLASS()
class UParticleModuleBeamTarget : public UParticleModule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	EBeamEndpointMethod Method = EBeamEndpointMethod::Distribution;

	UPROPERTY(EditAnywhere)
	FName ParameterName;

	// Distribution/UserSet: false = PSC local, true = world.
	UPROPERTY(EditAnywhere)
	bool bAbsolute = false;

	UPROPERTY(EditAnywhere)
	EBeamDistributionType DistributionType = EBeamDistributionType::Constant;

	// Keep the legacy property name for existing particle assets.
	UPROPERTY(EditAnywhere)
	FVector TargetPosition = FVector(5.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere)
	FParticleVectorRange UniformRange = FParticleVectorRange(FVector(5.0f, 0.0f, 0.0f));

	UPROPERTY(EditAnywhere)
	bool bLockOnSpawn = true;

	FInterpCurveFloat PositionX = FInterpCurveFloat::MakeLinear(5.0f, 5.0f);
	FInterpCurveFloat PositionY = FInterpCurveFloat::MakeLinear(0.0f, 0.0f);
	FInterpCurveFloat PositionZ = FInterpCurveFloat::MakeLinear(0.0f, 0.0f);

	FVector EvaluateDistribution(float EmitterTime) const;
	void GetCurves(TArray<FParticleCurveRef>& OutCurves) override;
	bool IsPropertyVisibleInDetails(const FProperty& Property) const override;
	using Super::Serialize;
	void Serialize(FStructuredArchive::FRecord Record) override;
};
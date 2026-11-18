#pragma once

#include "PrimitiveComponent.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleSystem.h"
#include "ParticleSystemComponent.generated.h"

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

private:
	bool GetEmitterBoundingBox(FBox& OutBoundingBox) const;
	int32 ClampLODLevel(int32 InLODLevel) const;
	bool TryDetermineLODLevelForLocation(const FVector& EffectLocation, int32& OutLODLevel) const;

	void InitializeMeshInstancingTest();

public:
	TArray<FParticleEmitterInstance*> EmitterInstances;
	UPROPERTY(EditAnywhere)
	UParticleSystem* Template = nullptr;

	UPROPERTY(EditAnywhere)
	float CustomTimeDilation = 1.0f;

	int32	LODLevel = 0;
	float	AccumLODDistanceCheckTime = 0.0f;
	bool	bPendingActivateLOD = false;
	UPROPERTY(EditAnywhere)
	bool	bOverrideLODMethod = false;

	UPROPERTY(EditAnywhere)
	EParticleSystemLODMethod LODMethod = EParticleSystemLODMethod::Automatic;

	// 테스트/디버깅 용
	bool	bMeshInstancingTest = true;

};
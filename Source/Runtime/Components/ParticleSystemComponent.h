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
	// Component-local positions; EmitterIndex indexes Template->Emitters (zero-based).
	// Overrides survive ResetSystem; SetTemplate clears them. LockOnSpawn is respected.
	bool SetBeamSourcePoint(int32 EmitterIndex, const FVector& Position);
	bool SetBeamTargetPoint(int32 EmitterIndex, const FVector& Position);
	bool ClearBeamSourcePoint(int32 EmitterIndex);
	bool ClearBeamTargetPoint(int32 EmitterIndex);
	FVector ResolveBeamSourcePoint(UParticleEmitter* Emitter, const FVector& DefaultPosition) const;
	FVector ResolveBeamTargetPoint(UParticleEmitter* Emitter, const FVector& DefaultPosition) const;
	void PostApplyProperties(const FProperty* ChangedProperty = nullptr) override;
	void SetLODLevel(int32 InLODLevel);
	int32 GetLODLevel() const { return LODLevel; }
	int32 DetermineLODLevelForLocation(const FVector& EffectLocation);
	EParticleSystemLODMethod GetCurrentLODMethod() const;

private:
	UParticleEmitter* GetBeamEmitterTemplate(int32 EmitterIndex) const;
	TMap<UParticleEmitter*, FVector> BeamSourceOverrides;
	TMap<UParticleEmitter*, FVector> BeamTargetOverrides;

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

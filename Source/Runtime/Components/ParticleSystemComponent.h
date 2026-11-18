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
	// Positions follow endpoint Absolute; EmitterIndex indexes Template->Emitters (zero-based).
	// Inputs are instance-owned. SetTemplate/ResetParticles clear them; restart preserves them.
	bool SetBeamSourcePoint(int32 EmitterIndex, const FVector& Position);
	bool SetBeamTargetPoint(int32 EmitterIndex, const FVector& Position);
	bool ClearBeamSourcePoint(int32 EmitterIndex);
	bool ClearBeamTargetPoint(int32 EmitterIndex);
	bool SetBeamActorParameter(const FString& Name, AActor* Actor);
	bool SetBeamComponentParameter(const FString& Name, USceneComponent* SceneComponent);
	bool ClearBeamParameter(const FString& Name);
	void PostApplyProperties(const FProperty* ChangedProperty = nullptr) override;
	void SetLODLevel(int32 InLODLevel);
	int32 GetLODLevel() const { return LODLevel; }
	int32 DetermineLODLevelForLocation(const FVector& EffectLocation);
	EParticleSystemLODMethod GetCurrentLODMethod() const;

private:
	UParticleEmitter* GetBeamEmitterTemplate(int32 EmitterIndex) const;
	FParticleBeamEmitterInstance* GetBeamInstance(int32 EmitterIndex);
	void EnsureEmitterInstances();
	// Explicit deactivation must also halt instances created later by setters.
	bool bSystemDeactivated = false;

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

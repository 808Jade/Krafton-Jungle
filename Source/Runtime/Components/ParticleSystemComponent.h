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

private:
	bool GetEmitterBoundingBox(FBox& OutBoundingBox) const;

	void InitializeMeshInstancingTest();

public:
	TArray<FParticleEmitterInstance*> EmitterInstances;
	UPROPERTY(EditAnywhere)
	UParticleSystem* Template = nullptr;

	UPROPERTY(EditAnywhere)
	float CustomTimeDilation = 1.0f;

	int32	LODLevel = 0;

	// 테스트/디버깅 용
	bool	bMeshInstancingTest = true;

};
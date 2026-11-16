#pragma once

#include "PrimitiveComponent.h"
#include "Particles/ParticleEmitterInstance.h"
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

private:
	bool GetEmitterBoundingBox(FBox& OutBoundingBox) const;

	void InitializeMeshInstancingTest();

public:
	TArray<FParticleEmitterInstance*> EmitterInstances;
	bool bUseFixedRelativeBoundingBox = false;
	FBox FixedRelativeBoundingBox{ FVector(-1.0f, -1.0f, -1.0f), FVector(1.0f, 1.0f, 1.0f) };
	//UParticleSystem* Template;

	// 테스트/디버깅 용
	bool bMeshInstancingTest = true;

	//TArray<FDynamicEmitterDataBase*> EmitterRenderData;

};
#pragma once

#include "GameFramework/Actor.h"

class AStaticMeshActor :public AActor
{
	DECLARE_CLASS(AStaticMeshActor, AActor)
public:
	AStaticMeshActor();
	virtual ~AStaticMeshActor() = default;

	virtual void BeginPlay() override;

	UPrimitiveComponent* GetPrimitiveComponent() const { return PrimitiveComponent; }

private:
	UPrimitiveComponent* PrimitiveComponent;
};


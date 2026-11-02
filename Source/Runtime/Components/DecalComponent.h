#pragma once

#include "SceneComponent.h"
#include "DecalComponent.generated.h"

UCLASS()
class UDecalComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	UDecalComponent() = default;
	virtual ~UDecalComponent() override = default;

	const FVector GetDecalSize() const { return DecalSize; }
	void SetDecalSize(const FVector& InSize) { DecalSize = InSize; }

	virtual FBox CalcLocalBounds() const override;

private:
	UPROPERTY(EditAnywhere)
	FVector DecalSize = FVector(1.f, 1.f, 1.f);
};
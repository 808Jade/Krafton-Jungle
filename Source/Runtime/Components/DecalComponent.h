#pragma once

#include "SceneComponent.h"
#include "DecalComponent.generated.h"

class FLineBatcher;
class UTexture2D;

UCLASS()
class UDecalComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	UDecalComponent();
	virtual ~UDecalComponent() override = default;

	const FVector GetDecalSize() const { return DecalSize; }
	void SetDecalSize(const FVector& InSize) { DecalSize = InSize; }

	virtual FBox CalcLocalBounds() const override;

	void DrawDebug(FLineBatcher* LineBatcher) const;

	UTexture2D* GetDecalTexture() const { return DecalTexture; }
	FMatrix GetWorldToDecal() const;

	virtual void OnRegister() override;
	virtual void PostEditChangeProperty(const FProperty& Property) override;
	FOBB GetDecalOBB() const;

	virtual void TickComponent(float DeltaTime) override;

	float GetFadeAlpha() const;

private:
	void ResolveTexture();

	UPROPERTY(EditAnywhere)
	FVector DecalSize = FVector(1.f, 1.f, 1.f);
	UPROPERTY(EditAnywhere)
	FString DecalTexturePath = "Assets/Checker.png";
	UPROPERTY(EditAnywhere)
	float FadeInStartDelay = 0.f;
	UPROPERTY(EditAnywhere)
	float FadeInDuration = 0.f;   
	UPROPERTY(EditAnywhere)
	float FadeStartDelay = 0.f;  
	UPROPERTY(EditAnywhere)
	float FadeDuration = 0.f;      

	float ElapsedTime = 0.f;       

	UTexture2D* DecalTexture = nullptr;
};
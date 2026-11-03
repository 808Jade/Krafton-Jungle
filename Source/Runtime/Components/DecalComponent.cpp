#include "EnginePCH.h"
#include "DecalComponent.h"
#include "Render/LineBatcher.h"
#include "Asset/AssetManager.h"

UDecalComponent::UDecalComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

FBox UDecalComponent::CalcLocalBounds() const
{
	return FBox{DecalSize * -1.f, DecalSize};
}

void UDecalComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
	LineBatcher->AddOrientedBox(CalcLocalBounds(), GetWorldMatrix(), FVector4(0.2f, 0.8f, 1.f, 1.f));
}

FMatrix UDecalComponent::GetWorldToDecal() const
{
	FMatrix ScaleMatrix = FMatrix::Identity;
	ScaleMatrix[0][0] = DecalSize.X;
	ScaleMatrix[1][1] = DecalSize.Y;
	ScaleMatrix[2][2] = DecalSize.Z;

	FMatrix DecalToWorld = ScaleMatrix * GetWorldMatrix();
	return DecalToWorld.Inverse();
}

void UDecalComponent::ResolveTexture()
{
	if (DecalTexturePath.empty())
	{
		DecalTexture = nullptr;
		return;
	}
	DecalTexture = UAssetManager::Get().LoadTexture(DecalTexturePath);
}

void UDecalComponent::OnRegister()
{
	Super::OnRegister();
	ResolveTexture();
}

void UDecalComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	if (Property.Name == "DecalTexturePath")
	{
		ResolveTexture();
	}
}

FOBB UDecalComponent::GetDecalOBB() const
{
	return FOBB::FromLocalBox(CalcLocalBounds(), GetWorldMatrix());
}

void UDecalComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	
	ElapsedTime += DeltaTime;
}

float UDecalComponent::GetFadeAlpha() const
{
	float FadeIn = 1.f;
	if (ElapsedTime < FadeInStartDelay)
	{
		FadeIn = 0.f;
	}
	else if (FadeInDuration > 0.f)
	{
		FadeIn = std::clamp((ElapsedTime - FadeInStartDelay) / FadeInDuration, 0.f, 1.f);
	}

	float FadeOut = 1.f;
	if (FadeDuration > 0.f)
	{
		FadeOut = std::clamp(1.f - (ElapsedTime - FadeStartDelay) / FadeDuration, 0.f, 1.f);
	}

	return FadeIn * FadeOut;
}
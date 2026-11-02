#include "EnginePCH.h"
#include "DecalComponent.h"
#include "Render/LineBatcher.h"
#include "Asset/AssetManager.h"

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

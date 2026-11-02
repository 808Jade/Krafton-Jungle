#include "EnginePCH.h"
#include "DecalComponent.h"
#include "Render/LineBatcher.h"

FBox UDecalComponent::CalcLocalBounds() const
{
	return FBox{DecalSize * -1.f, DecalSize};
}

void UDecalComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
	LineBatcher->AddOrientedBox(CalcLocalBounds(), GetWorldMatrix(), FVector4(0.2f, 0.8f, 1.f, 1.f));
}

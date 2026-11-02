#include "EnginePCH.h"
#include "DecalComponent.h"

FBox UDecalComponent::CalcLocalBounds() const
{
	return FBox{DecalSize * -1.f, DecalSize};
}

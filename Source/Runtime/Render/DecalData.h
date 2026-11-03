#pragma once

#include "Math/Matrix.h"
#include "Core/Stats/LightweightStats.h"

namespace DecalStats
{
	DECLARE_CYCLE_STAT("Decal Pass", STAT_DecalPass);
	DECLARE_CYCLE_STAT("Decal Culling", STAT_DecalCulling);
}

struct alignas(16) FDecalConstants
{
	FMatrix WorldToDecal;
	float FadeAlpha = 1.f;
	FVector Padding;
};
static_assert(sizeof(FDecalConstants) == 80);
#pragma once

#include "Math/Matrix.h"

struct alignas(16) FDecalConstants
{
	FMatrix WorldToDecal;
	float FadeAlpha = 1.f;
	FVector Padding;
};
static_assert(sizeof(FDecalConstants) == 80);
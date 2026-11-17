#pragma once

#include "Particles/ParticleTypes.h"

// bUseLocalSpace에 따라 로컬 또는 월드 좌표로 저장한다.
struct FBeamParticlePayload
{
	FVector SourcePosition = FVector(0.0f, 0.0f, 0.0f);
	FVector TargetPosition = FVector(0.0f, 0.0f, 0.0f);
};

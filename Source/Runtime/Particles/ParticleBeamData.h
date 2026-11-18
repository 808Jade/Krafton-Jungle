#pragma once

#include "Particles/ParticleTypes.h"

enum class EBeamEndpointMethod : uint8
{
	Distribution,
	UserSet,
	Actor,
	Component,
};

enum class EBeamDistributionType : uint8
{
	Constant,
	Uniform,
	ConstantCurve,
};

// bUseLocalSpace에 따라 로컬 또는 월드 좌표로 저장한다.
struct FBeamParticlePayload
{
	FVector SourcePosition = FVector(0.0f, 0.0f, 0.0f);
	FVector TargetPosition = FVector(0.0f, 0.0f, 0.0f);
};


inline constexpr int32 MaxBeamNoiseSegments = 16;
inline constexpr int32 MaxBeamNoisePoints = MaxBeamNoiseSegments + 1;
struct FBeamNoisePayload
{
	uint32 Seed = 1;
	uint32 Step = 0;

	int32 SegmentCount = 1;
	float Elapsed = 0.0f;

	// X/Y: Beam에 수직인 두 축 방향의 정규화된 변위.
	// Z는 사용하지 않는다.
	FVector FromOffsets[MaxBeamNoisePoints];
	FVector ToOffsets[MaxBeamNoisePoints];
};

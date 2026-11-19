#pragma once

#include "Particles/ParticleTypes.h"

inline constexpr int32 InvalidRibbonSlot = -1;

struct FRibbonParticlePayload
{
	// 활성 배열 인덱스가 아니라 ParticleData의 슬롯 번호.
	int32 PrevSlot = InvalidRibbonSlot;
	int32 NextSlot = InvalidRibbonSlot;

	// 이 점이 생성된 시점의 누적 이동 거리.
	// 오래된 점이 제거되어도 타일 UV의 기준을 유지한다.
	double DistanceFromStart = 0.0;
};

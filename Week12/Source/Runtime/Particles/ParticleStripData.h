#pragma once

#include "Particles/ParticleTypes.h"

// CPU에서 띠를 만들기 위한 중심점.
// GPU VertexBuffer에 그대로 넣는 구조체는 아니다.
struct FParticleStripPoint
{
	// bUseLocalSpace에 따라 로컬 또는 월드 공간.
	FVector Position = FVector(0.0f, 0.0f, 0.0f);

	// 전체 폭. 좌우 정점은 각각 Width * 0.5만큼 확장.
	float Width = 1.0f;

	FLinearColor Color = FLinearColor(
	    1.0f, 1.0f, 1.0f, 1.0f);

	// 길이 방향 텍스처 좌표.
	float U = 0.0f;
};

// Points 배열에서 독립적인 띠 하나가 차지하는 범위.
struct FParticleStripRange
{
	int32 FirstPoint = 0;
	int32 PointCount = 0;
};

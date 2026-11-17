#pragma once

#include "Particles/ParticleModule.h"

// 파티클마다 저장하는 SubUV 상태. 렌더러는 맨 앞의 Frame만 읽는다.
struct FParticleSubUVPayload
{
	// 지금 보여 줄 프레임 번호. 정수부를 쓴다. 왼쪽→오른쪽, 위→아래 순서이다.
	float Frame = 0.0f;
	// 태어날 때 정한 시작 프레임. (Movie 모듈이 쓴다)
	float StartOffset = 0.0f;
};

// 스프라이트 시트에서 어떤 칸을 그릴지 정하는 모듈의 베이스.
// 텍스처를 몇 칸으로 자를지는 Required의 SubImagesHorizontal/Vertical이 정하고,
// 이 모듈들은 그 칸 중 몇 번을 보여 줄지만 정한다.
class UParticleModuleSubUVBase : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleSubUVBase, UParticleModule)

public:
	UParticleModuleSubUVBase() = default;

	int32 GetRequiredPayloadBytes() const override { return static_cast<int32>(sizeof(FParticleSubUVPayload)); }

	// 이미터의 전체 칸 수(가로 x 세로). 분할 설정이 없으면 1이다.
	static int32 GetTotalFrames(const FParticleEmitterInstance& Owner);
};

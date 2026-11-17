#pragma once

#include "Particles/Modules/ParticleModuleSubUVBase.h"

// 파티클 수명에 비례해 프레임을 넘기는 모듈. (UE의 SubImage Index)
// 태어날 때 StartFrame, 죽을 때 EndFrame이고, 사이의 프레임이 수명을 똑같이 나눠 가진다.
// 예) 4칸을 0~3으로 두면 각 프레임이 수명의 25%씩 보인다. EndFrame이 StartFrame보다 작으면 거꾸로 재생한다.
class UParticleModuleSubUV : public UParticleModuleSubUVBase
{
	DECLARE_CLASS(UParticleModuleSubUV, UParticleModuleSubUVBase)
	REFLECT_START(UParticleModuleSubUV)
		PROPERTY(StartFrame)
		PROPERTY(EndFrame)
	REFLECT_END()

public:
	UParticleModuleSubUV() = default;

	void Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle) override;
	void Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime) override;
	bool NeedsSpawn() const override { return true; }
	bool NeedsUpdate() const override { return true; }

	// 처음 프레임 번호. 칸 수 범위로 잘린다.
	float StartFrame = 0.0f;

	// 마지막 프레임 번호. 음수이면 시트의 마지막 칸이다.
	float EndFrame = -1.0f;
};

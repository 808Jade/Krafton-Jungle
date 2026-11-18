#pragma once

#include "Particles/Modules/ParticleModuleSubUVBase.h"

// 정해진 프레임레이트로 프레임을 순서대로 넘기는 모듈. (UE의 SubUV Movie) 플립북 재생이다.
// 프레임은 파티클이 태어난 뒤 흐른 시간(초) x FrameRate로 정해서, 수명과 상관없이 같은 속도로 넘어간다.
class UParticleModuleSubUVMovie : public UParticleModuleSubUVBase
{
	DECLARE_CLASS(UParticleModuleSubUVMovie, UParticleModuleSubUVBase)
	REFLECT_START(UParticleModuleSubUVMovie)
		PROPERTY(FrameRate)
		PROPERTY(StartFrame)
		PROPERTY(bRandomStartFrame)
		PROPERTY(bLoop)
	REFLECT_END()

public:
	UParticleModuleSubUVMovie() = default;

	void Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle) override;
	void Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime) override;
	bool NeedsSpawn() const override { return true; }
	bool NeedsUpdate() const override { return true; }

	// 초당 넘기는 프레임 수. 음수이면 거꾸로 재생한다.
	float FrameRate = 12.0f;

	// 처음 프레임 번호.
	float StartFrame = 0.0f;

	// true이면 파티클마다 시작 프레임을 무작위로 정한다. (StartFrame은 무시)
	bool bRandomStartFrame = false;

	// true이면 끝까지 가면 처음부터 반복한다. false이면 마지막(거꾸로면 첫) 프레임에서 멈춘다.
	bool bLoop = true;
};

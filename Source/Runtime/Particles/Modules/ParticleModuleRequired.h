#pragma once

#include "Particles/ParticleModule.h"

// Material 프로퍼티 등록에 완전한 타입이 필요하다.
#include "Render/MaterialInterface.h"

// 에미터 전체에 적용되는 필수 설정. ReplayData를 채울 때 읽는다.
class UParticleModuleRequired : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleRequired, UParticleModule)
	REFLECT_START(UParticleModuleRequired)
		PROPERTY(Material)
		PROPERTY(bUseLocalSpace)
		PROPERTY(SortMode)
		PROPERTY(ScreenAlignment)
		PROPERTY(LockAxis)
		PROPERTY(EmitterDuration)
		PROPERTY(EmitterLoops)
		PROPERTY(MaxActiveParticles)
	REFLECT_END()

public:
	UParticleModuleRequired() = default;

	UMaterialInterface* Material = nullptr;

	bool bUseLocalSpace = false;
	EParticleSortMode SortMode = EParticleSortMode::None;

	// 스프라이트 에미터의 판 방향. Mesh 에미터는 무시한다.
	EParticleScreenAlignment ScreenAlignment = EParticleScreenAlignment::Rectangle;
	// ScreenAlignment가 AxisLock일 때만 사용한다. 엔진의 Up 축은 Z다.
	EParticleAxisLock LockAxis = EParticleAxisLock::None;

	float EmitterDuration = 1.0f;
	// 0이면 무한 반복.
	int32 EmitterLoops = 0;

	int32 MaxActiveParticles = 100;
};

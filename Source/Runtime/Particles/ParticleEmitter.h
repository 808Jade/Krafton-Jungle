#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Particles/ParticleTypes.h"

class UParticleLODLevel;
class UParticleModule;
class UParticleModuleSpawn;

class UParticleEmitter : public UObject
{
	DECLARE_CLASS(UParticleEmitter, UObject)

public:
	UParticleEmitter() = default;
	~UParticleEmitter() override = default;

	// 모듈들을 한 번 훑어서 런타임이 매번 다시 계산하지 않아도 되게 결과를 미리 계산해 멤버에 저장해 두는 함수
	// LODLevels[0]을 기준으로 아래 캐시 값을 계산한다.
	// 모듈이나 LODLevel 구성이 바뀐 뒤 다시 호출해야 한다.
	void CacheEmitterModuleInfo();

	TArray<UParticleLODLevel*> LODLevels;

	// ---- CacheEmitterModuleInfo 결과 ----

	// 파티클 슬롯 하나의 크기. sizeof(FBaseParticle) + 모듈 Payload 합.
	int32 ParticleSize = static_cast<int32>(sizeof(FBaseParticle));

	// 모듈별 Payload 오프셋. 슬롯 시작 기준 바이트.
	TMap<UParticleModule*, int32> ModuleOffsetMap;

	TArray<UParticleModule*> SpawnModules;
	TArray<UParticleModule*> UpdateModules;

	EDynamicEmitterType EmitterType = EDynamicEmitterType::Sprite;

	// 스폰 개수를 계산해 주는 모듈. NeedsSpawn()이 없어 SpawnModules에 들어가지 않으므로 따로 보관한다.
	// 없으면 nullptr (스폰하지 않는다).
	UParticleModuleSpawn* SpawnModule = nullptr;

	// 파티클 슬롯 개수. 스폰율 x 최대 수명 + 버스트 합으로 추정하고, Required의 MaxActiveParticles를 상한으로 한다.
	// FParticleEmitterInstance::Init에 그대로 넘기면 된다.
	int32 MaxActiveParticles = 100;
};

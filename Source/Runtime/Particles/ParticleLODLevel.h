#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"

class UParticleModule;
class UParticleModuleRequired;
class UParticleModuleTypeDataBase;
class UParticleModuleSpawn;

// 에미터의 LOD 한 단계. 현재는 Level 0만 실제로 사용한다.
class UParticleLODLevel : public UObject
{
	DECLARE_CLASS(UParticleLODLevel, UObject)

public:
	UParticleLODLevel() = default;
	~UParticleLODLevel() override = default;

	// Level, bEnabled, Required, TypeData, Modules를 저장한다. 모듈은 {Class, Data}로 쓰고
	// 불러올 때 클래스 이름으로 다시 만든다.
	using Super::Serialize;
	void Serialize(FStructuredArchive::FRecord Record) override;

	int32 CalculateMaxActiveParticleCount() const;

	int32 Level = 0;
	bool bEnabled = true;

	// 필수 설정. 소유권은 이 LODLevel에 있다.
	UParticleModuleRequired* RequiredModule = nullptr;

	// Required, TypeData를 제외한 일반 모듈 목록.
	TArray<UParticleModule*> Modules;

	// 없으면 Sprite 에미터.
	UParticleModuleTypeDataBase* TypeDataModule = nullptr;

	TArray<UParticleModule*>	SpawnModules;
	TArray<UParticleModule*>	UpdateModules;
	UParticleModuleSpawn*		SpawnModule = nullptr;
	bool						bModuleLayoutValid = false;
};

#include "EnginePCH.h"
#include "Particles/ParticleEmitter.h"

#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/Modules/ParticleModuleTypeDataBase.h"
#include "Particles/Modules/ParticleModuleTypeDataBeam.h"
#include "Particles/Modules/ParticleModuleRequired.h"
#include "Particles/Modules/ParticleModuleSpawn.h"
#include "Particles/Modules/ParticleModuleLifetime.h"
#include "UObject/Casts.h"

namespace
{
	// FParticleEmitterInstance::Init이 받아들이는 슬롯 수 범위(uint16 인덱스)와 같다.
	constexpr int32 MinParticleSlots = 1;
	constexpr int32 MaxParticleSlots = 65535;
	// Lifetime 모듈이 없을 때 쓰는 수명. 인스턴스의 기존 임시 기본값(1초)과 같다.
	constexpr float DefaultLifetime = 1.0f;
}

void UParticleEmitter::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);

	const bool bLoading = Record.GetUnderlyingArchive().IsLoading();

	int32 Num = 0;
	if (!bLoading)
	{
		for (UParticleLODLevel* LODLevel : LODLevels)
		{
			if (LODLevel)
			{
				++Num;
			}
		}
	}

	FStructuredArchive::FArray Array = Record.EnterArray("LODLevels", Num);
	if (bLoading)
	{
		LODLevels.Empty();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			UParticleLODLevel* LODLevel = NewObject<UParticleLODLevel>(this);
			LODLevel->Serialize(Array.EnterElement().EnterRecord());
			LODLevels.Add(LODLevel);
		}
	}
	else
	{
		for (UParticleLODLevel* LODLevel : LODLevels)
		{
			if (LODLevel)
			{
				LODLevel->Serialize(Array.EnterElement().EnterRecord());
			}
		}
	}
}

void UParticleEmitter::CacheEmitterModuleInfo()
{
	// 1. 리셋: 다시 호출해도 결과가 쌓이지 않게 먼저 비운다.
	ParticleSize = static_cast<int32>(sizeof(FBaseParticle));
	ModuleOffsetMap.Empty();
	SpawnModules.Empty();
	UpdateModules.Empty();
	EmitterType = EDynamicEmitterType::Sprite;
	SpawnModule = nullptr;
	MaxActiveParticles = 100;

	// 2. 기준이 되는 LODLevels[0]이 없으면 기본값 그대로 끝낸다.
	if (LODLevels.Num() == 0 || !LODLevels[0])
	{
		return;
	}
	UParticleLODLevel* LODLevel = LODLevels[0];

	// 3. 에미터 종류: TypeData 모듈이 없으면 Sprite, 있으면 그 모듈이 알려준다.
	if (LODLevel->TypeDataModule)
	{
		EmitterType = LODLevel->TypeDataModule->GetEmitterType();
	}

	// 4. 훑을 목록: 일반 모듈들 + TypeData 모듈 (TypeData는 Modules에 들어 있지 않다)
	TArray<UParticleModule*> AllModules;
	for (UParticleModule* Module : LODLevel->Modules)
	{
		AllModules.Add(Module);
	}
	if (LODLevel->TypeDataModule)
	{
		AllModules.Add(LODLevel->TypeDataModule);
	}

	// 5. 모듈마다 payload 자리를 배정하고, Spawn/Update 대상이면 목록에 담는다.
	int32 Offset = static_cast<int32>(sizeof(FBaseParticle));
	float MaxLifetime = DefaultLifetime;
	for (UParticleModule* Module : AllModules)
	{
		if (!Module || !Module->bEnabled)
		{
			continue;
		}

		// Spawn 모듈은 NeedsSpawn()이 없으니 따로 찾아 둔다. 여러 개면 첫 번째만 쓴다.
		if (!SpawnModule)
		{
			SpawnModule = Cast<UParticleModuleSpawn>(Module);
		}
		// 수명 모듈이 여러 개면 가장 마지막 Spawn 호출이 덮어쓰므로, 같은 규칙으로 마지막 것을 쓴다.
		if (UParticleModuleLifetime* LifetimeModule = Cast<UParticleModuleLifetime>(Module))
		{
			MaxLifetime = LifetimeModule->GetMaxLifetime();
		}

		const int32 PayloadBytes = Module->GetRequiredPayloadBytes();
		if (PayloadBytes > 0)
		{
			ModuleOffsetMap.Add(Module, Offset);
			Offset += PayloadBytes;
		}

		if (Module->NeedsSpawn())
		{
			SpawnModules.Add(Module);
		}
		if (Module->NeedsUpdate())
		{
			UpdateModules.Add(Module);
		}
	}

	// 6. 슬롯 하나의 최종 크기 = FBaseParticle + 모든 payload
	ParticleSize = Offset;

	// 7. 슬롯 개수 추정: 동시에 살아 있을 수 있는 최대 수 = 스폰율 x 최대 수명 (+ 버스트 전부)
	float Estimate = 0.0f;
	if (SpawnModule)
	{
		Estimate = std::max(SpawnModule->SpawnRate, 0.0f) * std::max(MaxLifetime, 0.0f);
		for (const FParticleBurst& Burst : SpawnModule->Bursts)
		{
			Estimate += static_cast<float>(std::max(Burst.Count, 0));
		}
	}
	// 올림 후 +1: 틱 경계에서 소수 부분 때문에 하나 더 살아 있을 수 있다.
	int32 Slots = static_cast<int32>(std::ceil(Estimate)) + 1;

	// Required의 MaxActiveParticles는 사용자가 정한 상한이다.
	if (LODLevel->RequiredModule && LODLevel->RequiredModule->MaxActiveParticles > 0)
	{
		Slots = std::min(Slots, LODLevel->RequiredModule->MaxActiveParticles);
	}
	MaxActiveParticles = std::clamp(Slots, MinParticleSlots, MaxParticleSlots);

	if (const auto& BeamTypeData =
	        Cast<UParticleModuleTypeDataBeam>(LODLevel->TypeDataModule))
	{
		int32 BeamCapacity = std::clamp(
		    BeamTypeData->MaxBeamCount,
		    MinParticleSlots,
		    MaxParticleSlots);

		if (LODLevel->RequiredModule &&
			LODLevel->RequiredModule->MaxActiveParticles > 0)
		{
			BeamCapacity = std::min(
			    BeamCapacity,
			    LODLevel->RequiredModule->MaxActiveParticles);
		}

		MaxActiveParticles = BeamCapacity;
	}
}

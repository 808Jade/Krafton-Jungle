#include "EnginePCH.h"
#include "Particles/ParticleEmitter.h"

#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleReplayData.h"
#include "Particles/Modules/ParticleModuleTypeDataBase.h"
#include "Particles/Modules/ParticleModuleTypeDataBeam.h"
#include "Particles/Modules/ParticleModuleTypeDataRibbon.h"
#include "Particles/Modules/ParticleModuleRequired.h"
#include "Particles/Modules/ParticleModuleSpawn.h"
#include "UObject/Casts.h"

namespace
{
	// FParticleEmitterInstance::Init이 받아들이는 슬롯 수 범위(uint16 인덱스)와 같다.
	constexpr int32 MinParticleSlots = 1;
	constexpr int32 MaxParticleSlots = 65535;

	TArray<UParticleModule*> GatherModules(const UParticleLODLevel* LODLevel)
	{
		TArray<UParticleModule*> AllModules;
		for (UParticleModule* Module : LODLevel->Modules)
		{
			AllModules.Add(Module);
		}
		if (LODLevel->TypeDataModule)
		{
			AllModules.Add(LODLevel->TypeDataModule);
		}
		return AllModules;
	}

	bool HasSameModuleLayout(const TArray<UParticleModule*>& HighestModules, const TArray<UParticleModule*>& Modules)
	{
		if (HighestModules.Num() != Modules.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Modules.Num(); ++Index)
		{
			const UParticleModule* Highest = HighestModules[Index];
			const UParticleModule* Module = Modules[Index];
			if (!Highest || !Module)
			{
				if (Highest != Module)
				{
					return false;
				}
				continue;
			}
			if (Highest->GetClass() != Module->GetClass())
			{
				return false;
			}
		}
		return true;
	}

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
	ParticleSize = static_cast<int32>(sizeof(FBaseParticle));
	ModuleOffsetMap.Empty();
	EmitterType = EDynamicEmitterType::Sprite;
	MaxActiveParticles = 100;

	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (LODLevel)
		{
			LODLevel->SpawnModules.Empty();
			LODLevel->UpdateModules.Empty();
			LODLevel->SpawnModule = nullptr;
			LODLevel->bModuleLayoutValid = false;
		}
	}

	if (LODLevels.Num() == 0 || !LODLevels[0])
	{
		return;
	}
	const UParticleLODLevel* HighestLODLevel = LODLevels[0];

	if (HighestLODLevel->TypeDataModule)
	{
		EmitterType = HighestLODLevel->TypeDataModule->GetEmitterType();
	}

	const TArray<UParticleModule*> HighestModules = GatherModules(HighestLODLevel);
	TArray<int32> ModuleOffsets;
	int32 Offset = static_cast<int32>(sizeof(FBaseParticle));
	for (UParticleModule* Module : HighestModules)
	{
		const int32 PayloadBytes = Module ? Module->GetRequiredPayloadBytes() : 0;
		if (PayloadBytes > 0)
		{
			ModuleOffsets.Add(Offset);
			Offset += PayloadBytes;
		}
		else
		{
			ModuleOffsets.Add(InvalidPayloadOffset);
		}
	}
	ParticleSize = Offset;

	int32 Slots = 0;
	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); ++LODIndex)
	{
		UParticleLODLevel* LODLevel = LODLevels[LODIndex];
		if (!LODLevel)
		{
			HTR_LOG(Warning, "CacheEmitterModuleInfo: LOD {} is null", LODIndex);
			continue;
		}

		const TArray<UParticleModule*> Modules = GatherModules(LODLevel);
		if (!HasSameModuleLayout(HighestModules, Modules))
		{
			HTR_LOG(Warning, "CacheEmitterModuleInfo: LOD {} module layout differs from LOD 0", LODIndex);
			continue;
		}
		LODLevel->bModuleLayoutValid = true;

		for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ++ModuleIndex)
		{
			UParticleModule* Module = Modules[ModuleIndex];
			if (!Module)
			{
				continue;
			}

			if (ModuleOffsets[ModuleIndex] != InvalidPayloadOffset)
			{
				ModuleOffsetMap.Add(Module, ModuleOffsets[ModuleIndex]);
			}
			if (!LODLevel->SpawnModule)
			{
				LODLevel->SpawnModule = Cast<UParticleModuleSpawn>(Module);
			}
			if (Module->NeedsSpawn())
			{
				LODLevel->SpawnModules.Add(Module);
			}
			if (Module->NeedsUpdate())
			{
				LODLevel->UpdateModules.Add(Module);
			}
		}

		std::stable_sort(LODLevel->UpdateModules.begin(), LODLevel->UpdateModules.end(),
			[](const UParticleModule* A, const UParticleModule* B) { return A->GetUpdateOrder() < B->GetUpdateOrder(); });

		if (LODLevel->bEnabled)
		{
			Slots = std::max(Slots, LODLevel->CalculateMaxActiveParticleCount());
		}
	}

	MaxActiveParticles = std::clamp(Slots, MinParticleSlots, MaxParticleSlots);

	if (const auto& BeamTypeData =
	        Cast<UParticleModuleTypeDataBeam>(HighestLODLevel->TypeDataModule))
	{
		int32 BeamCapacity = std::clamp(
		    BeamTypeData->MaxBeamCount,
		    MinParticleSlots,
		    MaxParticleSlots);

		if (HighestLODLevel->RequiredModule &&
			HighestLODLevel->RequiredModule->MaxActiveParticles > 0)
		{
			BeamCapacity = std::min(
			    BeamCapacity,
			    HighestLODLevel->RequiredModule->MaxActiveParticles);
		}

		MaxActiveParticles = BeamCapacity;
	}

	if (const auto* RibbonTypeData =
	        Cast<UParticleModuleTypeDataRibbon>(
	            LODLevel->TypeDataModule))
	{
		int32 Capacity = std::clamp(
		    RibbonTypeData->MaxPointsPerTrail,
		    2,
		    MaxParticleSlots);

		if (LODLevel->RequiredModule &&
		    LODLevel->RequiredModule->MaxActiveParticles > 0)
		{
			Capacity = std::min(
			    Capacity,
			    LODLevel->RequiredModule->MaxActiveParticles);
		}

		MaxActiveParticles = Capacity;
	}
}

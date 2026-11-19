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
	            HighestLODLevel->TypeDataModule))
	{
		int32 Capacity = std::clamp(
		    RibbonTypeData->MaxPointsPerTrail,
		    2,
		    MaxParticleSlots);

		if (HighestLODLevel->RequiredModule &&
		    HighestLODLevel->RequiredModule->MaxActiveParticles > 0)
		{
			Capacity = std::min(
			    Capacity,
			    HighestLODLevel->RequiredModule->MaxActiveParticles);
		}

		MaxActiveParticles = Capacity;
	}
}

UParticleLODLevel* UParticleEmitter::DuplicateLODLevel(const int32 SourceIndex)
{
	if (LODLevels.IsEmpty())
		return nullptr;
	UParticleLODLevel* Source = LODLevels[std::clamp(SourceIndex, 0, LODLevels.Num() - 1)];
	return Source ? Source->Duplicate(this) : nullptr;
}

void UParticleEmitter::InsertLODLevel(const int32 InsertIndex, UParticleLODLevel* LODLevel)
{
	if (!LODLevel)
		return;
	const int32 ClampedIndex = std::clamp(InsertIndex, 0, LODLevels.Num());
	LODLevels.Insert(LODLevel, static_cast<uint32>(ClampedIndex));
	RenumberLODLevels();
}

UParticleLODLevel* UParticleEmitter::DetachLODLevel(const int32 LODIndex)
{
	if (LODIndex < 0 || LODIndex >= LODLevels.Num())
		return nullptr;
	UParticleLODLevel* Removed = LODLevels[LODIndex];
	LODLevels.RemoveAt(static_cast<uint32>(LODIndex));
	RenumberLODLevels();
	return Removed;
}

bool UParticleEmitter::AutogenerateLowestLODLevel(const bool bDuplicateHighest, TArray<UParticleLODLevel*>& OutDetachedLODLevels)
{
	if (LODLevels.IsEmpty() || !LODLevels[0])
		return false;

	while (LODLevels.Num() > 1)
	{
		const uint32 LastIndex = static_cast<uint32>(LODLevels.Num() - 1);
		OutDetachedLODLevels.Add(LODLevels[LastIndex]);
		LODLevels.RemoveAt(LastIndex);
	}

	UParticleLODLevel* Source = LODLevels[0];
	UParticleLODLevel* Copy = Source->Duplicate(this);
	if (!Copy)
		return false;

	const bool bBeam = Cast<UParticleModuleTypeDataBeam>(Source->TypeDataModule) != nullptr;
	const bool bRebon = Cast<UParticleModuleTypeDataRibbon>(Source->TypeDataModule) != nullptr;
	const float Percentage = (bDuplicateHighest || bBeam || bRebon) ? 100.0f : 10.0f;
	if (Percentage < 100.0f)
		Copy->GenerateFromLODLevel(Source, Percentage);

	LODLevels.Add(Copy);
	RenumberLODLevels();
	return true;
}

void UParticleEmitter::RenumberLODLevels()
{
	for (int32 Index = 0; Index < LODLevels.Num(); ++Index)
	{
		if (LODLevels[Index])
			LODLevels[Index]->Level = Index;
	}
}

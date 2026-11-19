#include "EnginePCH.h"
#include "Particles/ParticleLODLevel.h"

#include "Particles/ParticleModule.h"
#include "Particles/Modules/ParticleModuleRequired.h"
#include "Particles/Modules/ParticleModuleTypeDataBase.h"
#include "Particles/Modules/ParticleModuleSpawn.h"
#include "Particles/Modules/ParticleModuleLifetime.h"
#include "UObject/Casts.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectGlobals.h"

#include <algorithm>

namespace
{
	// 모듈 하나를 {Class, Data}로 저장하고 불러온다.
	// 저장: Module이 nullptr이면 호출하지 않는다.
	// 불러오기: 클래스 이름으로 Outer 아래에 모듈을 새로 만들고 Data를 읽는다.
	//           알 수 없는 클래스면 경고만 남기고 Module을 nullptr로 둔다.
	void SerializeModule(FStructuredArchive::FSlot Slot, UParticleModule*& Module, UObject* Outer)
	{
		FStructuredArchive::FRecord Record = Slot.EnterRecord();
		const bool bLoading = Record.GetUnderlyingArchive().IsLoading();

		FString ClassName;
		if (!bLoading && Module)
		{
			ClassName = Module->GetClass()->GetName();
		}
		Record << SA_VALUE("Class", ClassName);

		if (bLoading)
		{
			Module = nullptr;
			UClass* Class = FindClass(ClassName);
			if (!Class || !Class->IsChildOf(UParticleModule::StaticClass()) || !Class->CanCreateInstance())
			{
				HTR_LOG(Warning, "Load: unknown particle module class '{}', skipped", ClassName);
				return;
			}
			Module = NewObject<UParticleModule>(Outer, Class);
		}

		if (Module)
		{
			Module->Serialize(Record.EnterRecord("Data"));
		}
	}

	// 선택 모듈(Required, TypeData) 필드. 저장 때는 모듈이 있을 때만 필드를 쓴다.
	// 파일에 필드가 없으면 Module은 바뀌지 않는다.
	void SerializeOptionalModule(FStructuredArchive::FRecord& Record, const char* FieldName, UParticleModule*& Module, UObject* Outer)
	{
		TOptional<FStructuredArchive::FSlot> Slot = Record.TryEnterField(FieldName, Module != nullptr);
		if (Slot.IsSet())
		{
			SerializeModule(Slot.GetValue(), Module, Outer);
		}
	}
}

void UParticleLODLevel::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);

	const bool bLoading = Record.GetUnderlyingArchive().IsLoading();

	Record << SA_VALUE("Level", Level) << SA_VALUE("bEnabled", bEnabled);

	// 선택 모듈은 UParticleModule*로 받아 읽은 뒤 맞는 타입일 때만 멤버에 넣는다.
	UParticleModule* Required = RequiredModule;
	SerializeOptionalModule(Record, "Required", Required, this);
	if (bLoading)
	{
		RequiredModule = Cast<UParticleModuleRequired>(Required);
	}

	UParticleModule* TypeData = TypeDataModule;
	SerializeOptionalModule(Record, "TypeData", TypeData, this);
	if (bLoading)
	{
		TypeDataModule = Cast<UParticleModuleTypeDataBase>(TypeData);
	}

	// nullptr 모듈은 저장하지 않는다.
	int32 Num = 0;
	if (!bLoading)
	{
		for (UParticleModule* Module : Modules)
		{
			if (Module)
			{
				++Num;
			}
		}
	}

	FStructuredArchive::FArray Array = Record.EnterArray("Modules", Num);
	if (bLoading)
	{
		Modules.Empty();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			UParticleModule* Module = nullptr;
			SerializeModule(Array.EnterElement(), Module, this);
			if (Module)
			{
				Modules.Add(Module);
			}
		}
	}
	else
	{
		for (UParticleModule* Module : Modules)
		{
			if (Module)
			{
				SerializeModule(Array.EnterElement(), Module, this);
			}
		}
	}
}

UParticleLODLevel* UParticleLODLevel::Duplicate(UObject* Outer)
{
	return Cast<UParticleLODLevel>(StaticDuplicateObject(this, Outer));
}

void UParticleLODLevel::GenerateFromLODLevel(const UParticleLODLevel* SourceLODLevel, const float Percentage)
{
	if (!SourceLODLevel)
		return;

	const int32 ModuleCount = std::min(Modules.Num(), SourceLODLevel->Modules.Num());
	for (int32 ModuleIndex = 0; ModuleIndex < ModuleCount; ++ModuleIndex)
	{
		UParticleModule* DestModule = Modules[ModuleIndex];
		const UParticleModule* SourceModule = SourceLODLevel->Modules[ModuleIndex];
		if (DestModule && SourceModule)
			DestModule->GenerateLODModuleValues(SourceModule, Percentage);
	}
}

void UParticleLODLevel::Destroy(UParticleLODLevel* LODLevel)
{
	if (!LODLevel)
		return;
	for (UParticleModule* Module : LODLevel->Modules)
		delete Module;
	delete LODLevel->RequiredModule;
	delete LODLevel->TypeDataModule;
	delete LODLevel;
}

int32 UParticleLODLevel::CalculateMaxActiveParticleCount() const
{
	float MaxLifetime = 0.0f;
	bool bHasLifetimeModule = false;
	bool bReceivesEvents = false;
	for (UParticleModule* Module : Modules)
	{
		if (!Module || !Module->bEnabled)
		{
			continue;
		}
		bReceivesEvents |= Module->ReceivesParticleEvents();
		if (UParticleModuleLifetime* LifetimeModule = Cast<UParticleModuleLifetime>(Module))
		{
			MaxLifetime += LifetimeModule->GetMaxLifetime();
			bHasLifetimeModule = true;
		}
	}
	if (!bHasLifetimeModule)
	{
		MaxLifetime = 1.0f;
	}

	float Estimate = 0.0f;
	if (SpawnModule && SpawnModule->bEnabled)
	{
		Estimate = std::max(SpawnModule->SpawnRate, 0.0f) * std::max(MaxLifetime, 0.0f);
		for (const FParticleBurst& Burst : SpawnModule->Bursts)
		{
			Estimate += static_cast<float>(std::max(Burst.Count, 0));
		}
	}
	int32 Slots = static_cast<int32>(std::ceil(Estimate)) + 1;

	// 충돌 이벤트로 스폰하는 이미터는 스폰 비율로 필요한 수를 알 수 없다. 사용자가 정한 MaxActiveParticles를 용량으로 쓴다.
	if (bReceivesEvents)
	{
		constexpr int32 DefaultEventSpawnSlots = 100;
		Slots = std::max(Slots, RequiredModule && RequiredModule->MaxActiveParticles > 0 ? RequiredModule->MaxActiveParticles : DefaultEventSpawnSlots);
	}

	if (RequiredModule && RequiredModule->MaxActiveParticles > 0)
	{
		Slots = std::min(Slots, RequiredModule->MaxActiveParticles);
	}
	return Slots;
}

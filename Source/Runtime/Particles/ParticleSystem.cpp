#include "EnginePCH.h"
#include "Particles/ParticleSystem.h"

#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Serialization/MathSerialization.h"

#include <algorithm>

namespace
{
	constexpr float DefaultLODDistanceStep = 25.0f;

	float MakeAppendedLODDistance(const TArray<float>& Distances)
	{
		const int32 Num = Distances.Num();
		if (Num == 0)
			return 0.0f;
		if (Num == 1)
			return Distances[0] + DefaultLODDistanceStep;
		const float Step = Distances[Num - 1] - Distances[Num - 2];
		return Distances[Num - 1] + (Step > 0.0f ? Step : DefaultLODDistanceStep);
	}
}

void UParticleSystem::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);

	const bool bLoading = Record.GetUnderlyingArchive().IsLoading();

	// 에셋 단위 설정. 파일에 없으면 기본값을 그대로 둔다.
	Record << SA_VALUE("bUseFixedRelativeBoundingBox", bUseFixedRelativeBoundingBox);
	{
		FStructuredArchive::FRecord BoxRecord = Record.EnterRecord("FixedRelativeBoundingBox");
		BoxRecord << SA_VALUE("Min", FixedRelativeBoundingBox.Min) << SA_VALUE("Max", FixedRelativeBoundingBox.Max);
	}

	int32 LODMethodValue = static_cast<int32>(LODMethod);
	Record << SA_VALUE("LODMethod", LODMethodValue);
	if (bLoading)
	{
		LODMethod = static_cast<EParticleSystemLODMethod>(LODMethodValue);
	}
	Record << SA_VALUE("LODDistanceCheckTime", LODDistanceCheckTime);
	{
		int32 LODDistanceNum = LODDistances.Num();
		FStructuredArchive::FArray LODDistanceArray = Record.EnterArray("LODDistances", LODDistanceNum);
		if (bLoading)
		{
			LODDistances.SetNum(LODDistanceNum);
		}
		for (int32 Index = 0; Index < LODDistanceNum; ++Index)
		{
			LODDistanceArray << LODDistances[Index];
		}
	}

	int32 Num = 0;
	if (!bLoading)
	{
		for (UParticleEmitter* Emitter : Emitters)
		{
			if (Emitter)
			{
				++Num;
			}
		}
	}

	FStructuredArchive::FArray Array = Record.EnterArray("Emitters", Num);
	if (bLoading)
	{
		Emitters.Empty();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			UParticleEmitter* Emitter = NewObject<UParticleEmitter>(this);
			Emitter->Serialize(Array.EnterElement().EnterRecord());
			Emitters.Add(Emitter);
		}
	}
	else
	{
		for (UParticleEmitter* Emitter : Emitters)
		{
			if (Emitter)
			{
				Emitter->Serialize(Array.EnterElement().EnterRecord());
			}
		}
	}
}

bool UParticleSystem::NormalizeLODSetup()
{
	int32 TargetCount = std::max(1, LODDistances.Num());
	for (const UParticleEmitter* Emitter : Emitters)
	{
		if (Emitter)
			TargetCount = std::max(TargetCount, Emitter->LODLevels.Num());
	}

	bool bChanged = false;
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (!Emitter || Emitter->LODLevels.IsEmpty())
			continue;
		while (Emitter->LODLevels.Num() < TargetCount)
		{
			UParticleLODLevel* Copy = Emitter->DuplicateLODLevel(Emitter->LODLevels.Num() - 1);
			if (!Copy)
				break;
			Emitter->LODLevels.Add(Copy);
			bChanged = true;
		}
		Emitter->RenumberLODLevels();
	}

	while (LODDistances.Num() < TargetCount)
	{
		LODDistances.Add(MakeAppendedLODDistance(LODDistances));
		bChanged = true;
	}
	if (LODDistances[0] != 0.0f)
	{
		LODDistances[0] = 0.0f;
		bChanged = true;
	}
	for (int32 Index = 1; Index < LODDistances.Num(); ++Index)
	{
		if (LODDistances[Index] < LODDistances[Index - 1])
		{
			LODDistances[Index] = LODDistances[Index - 1];
			bChanged = true;
		}
	}
	return bChanged;
}

bool UParticleSystem::InsertLOD(const int32 SourceIndex, const int32 InsertIndex)
{
	if (InsertIndex < 1)
		return false;

	NormalizeLODSetup();

	TArray<UParticleLODLevel*> Copies;
	for (UParticleEmitter* Emitter : Emitters)
	{
		UParticleLODLevel* Copy = nullptr;
		if (Emitter && !Emitter->LODLevels.IsEmpty())
		{
			Copy = Emitter->DuplicateLODLevel(SourceIndex);
			if (!Copy)
			{
				for (UParticleLODLevel* Created : Copies)
					UParticleLODLevel::Destroy(Created);
				return false;
			}
		}
		Copies.Add(Copy);
	}

	for (int32 EmitterIndex = 0; EmitterIndex < Emitters.Num(); ++EmitterIndex)
	{
		if (Emitters[EmitterIndex] && Copies[EmitterIndex])
			Emitters[EmitterIndex]->InsertLODLevel(InsertIndex, Copies[EmitterIndex]);
	}

	const int32 DistanceIndex = std::min(InsertIndex, LODDistances.Num());
	const float NewDistance = LODDistances[DistanceIndex - 1];
	LODDistances.Insert(NewDistance, static_cast<uint32>(DistanceIndex));
	return true;
}

bool UParticleSystem::DetachLOD(const int32 LODIndex, TArray<UParticleLODLevel*>& OutDetachedLODLevels)
{
	if (LODIndex < 1)
		return false;

	NormalizeLODSetup();

	for (UParticleEmitter* Emitter : Emitters)
	{
		if (!Emitter)
			continue;
		if (UParticleLODLevel* Detached = Emitter->DetachLODLevel(LODIndex))
			OutDetachedLODLevels.Add(Detached);
	}
	if (LODIndex < LODDistances.Num())
		LODDistances.RemoveAt(static_cast<uint32>(LODIndex));
	return true;
}

bool UParticleSystem::RegenerateLowestLOD(const bool bDuplicateHighest, TArray<UParticleLODLevel*>& OutDetachedLODLevels)
{
	NormalizeLODSetup();

	for (UParticleEmitter* Emitter : Emitters)
	{
		if (Emitter)
			Emitter->AutogenerateLowestLODLevel(bDuplicateHighest, OutDetachedLODLevels);
	}

	LODDistances.Empty();
	LODDistances.Add(0.0f);
	LODDistances.Add(DefaultLODDistanceStep);
	return true;
}

#include "EnginePCH.h"
#include "Particles/ParticleSystem.h"

#include "Particles/ParticleEmitter.h"
#include "Serialization/MathSerialization.h"

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

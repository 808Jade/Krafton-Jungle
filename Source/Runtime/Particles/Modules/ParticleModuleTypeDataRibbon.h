#pragma once

#include "Particles/Modules/ParticleModuleTypeDataBase.h"
#include "Particles/ParticleRibbonData.h"

#include "ParticleModuleTypeDataRibbon.generated.h"

UCLASS()
class UParticleModuleTypeDataRibbon : public UParticleModuleTypeDataBase
{
	GENERATED_BODY()

public:
	EDynamicEmitterType GetEmitterType() const override
	{
		return EDynamicEmitterType::Ribbon;
	}

	int32 GetRequiredPayloadBytes() const override
	{
		return static_cast<int32>(sizeof(FRibbonParticlePayload));
	}
	
	UPROPERTY()
	// 서로 독립적인 Ribbon 경로 개수.
	int32 MaxTrailCount = 1;

	UPROPERTY()
	// 경로 하나가 유지할 최대 점 개수.
	int32 MaxPointsPerTrail = 128;

	UPROPERTY()
	// 새 점을 추가하기 위한 최소 이동 거리.
	float MinSpawnDistance = 0.1f;

	UPROPERTY()
	// 0이면 현재 Ribbon 전체에 U = 0~1을 적용.
	float TextureTileDistance = 0.0f;
};

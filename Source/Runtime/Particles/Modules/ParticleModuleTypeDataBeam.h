#pragma once

#include "Particles/Modules/ParticleModuleTypeDataBase.h"
#include "Particles/ParticleBeamData.h"

#include "ParticleModuleTypeDataBeam.generated.h"

UCLASS()
class UParticleModuleTypeDataBeam : public UParticleModuleTypeDataBase
{
	GENERATED_BODY()

public:
	EDynamicEmitterType GetEmitterType() const override
	{
		return EDynamicEmitterType::Beam;
	}

	int32 GetRequiredPayloadBytes() const override
	{
		return static_cast<int32>(sizeof(FBeamParticlePayload));
	}

	UPROPERTY()
	// 동시에 유지할 Beam 수.
	int32 MaxBeamCount = 1;

	UPROPERTY()
	// Beam 하나를 구성하는 구간 수.
	// 중심점 개수는 SegmentCount + 1.
	int32 SegmentCount = 1;
	
	UPROPERTY()
	// 시뮬레이션 공간에서 텍스처가 한 번 반복되는 거리.
	// 0이면 Beam 전체에 U = 0~1을 적용.
	float TextureTileDistance = 0.0f;
};
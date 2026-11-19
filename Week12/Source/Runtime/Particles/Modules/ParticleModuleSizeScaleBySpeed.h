#pragma once

#include "Particles/ParticleModule.h"

// 파티클의 속도에 따라 크기를 늘리거나 줄이는 모듈. 불똥이 빠를수록 길게 늘어나는 효과에 쓴다.
// 축마다 factor = clamp(1 + SpeedScale * 속도, 0, MaxScale)를 구해 현재 Size에 곱한다.
// - 속도가 0이면 크기는 그대로다. SpeedScale이 음수이면 빠를수록 작아진다.
// - 스프라이트의 Velocity 정렬에서는 Size.Y가 속도 방향 길이다. 메시 파티클은 X·Y·Z가 메시의 각 축 스케일이다.
// - Size 모듈(Size over life)의 결과 위에 곱하려고 순서 100에서 돈다. 충돌(1000)보다는 앞이다.
class UParticleModuleSizeScaleBySpeed : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleSizeScaleBySpeed, UParticleModule)
	REFLECT_START(UParticleModuleSizeScaleBySpeed)
		PROPERTY(SpeedScale)
		PROPERTY(MaxScale)
		PROPERTY(LengthPerSpeed)
		PROPERTY(MaxLength)
	REFLECT_END()

public:
	UParticleModuleSizeScaleBySpeed() = default;

	void Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime) override;
	bool NeedsUpdate() const override { return true; }
	int32 GetUpdateOrder() const override { return 100; }

	// 속도 1m/s당 크기에 더하는 배율. 축마다 따로 정한다. (0이면 그 축은 변하지 않는다)
	// 기본값은 Y만 늘린다. 스프라이트를 Velocity 정렬로 두면 속도 방향으로 길어진다.
	FVector SpeedScale = FVector(0.0f, 0.1f, 0.0f);

	// 축마다 최종 배율의 상한. 너무 길게 늘어나는 것을 막는다.
	FVector MaxScale = FVector(5.0f, 5.0f, 5.0f);

	// 속도 1m/s당 Size.Y에 더하는 길이(m). 기본 크기와 상관없이 속도에 비례해 길어진다. 0이면 쓰지 않는다.
	// 위 배율을 곱한 뒤에 더한다. 스프라이트를 Velocity 정렬로 두면 Size.Y가 날아가는 방향 길이다.
	// 예) 0.05이면 속도 10m/s에서 길이가 0.5m 늘어난다.
	float LengthPerSpeed = 0.0f;

	// Size.Y의 최종 상한(m). 0이면 제한이 없다. 위 배율과 LengthPerSpeed를 모두 적용한 결과에 건다.
	float MaxLength = 0.0f;
};

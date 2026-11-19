#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleSize.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleHelper.h"
#include "Serialization/MathSerialization.h"

#include <cmath>

void UParticleModuleSize::Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle)
{
	const FVector S = StartSize.Sample();
	Particle.Size += S;
	Particle.BaseSize += S;
}

void UParticleModuleSize::Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime)
{
	if (!bUseSizeOverLife) return; // 리셋으로 이미 Size == BaseSize라 할 일이 없음

	for (int32 i = 0; i < Owner.ActiveParticles; ++i)
	{
		DECLARE_PARTICLE_PTR(Particle, Owner.ParticleData + Owner.ParticleStride * Owner.ParticleIndices[i]);
		if (Particle->Flags & STATE_Particle_Freeze) continue;

		const float T = std::clamp(Particle->RelativeTime, 0.0f, 1.0f);
		Particle->Size.X = Particle->BaseSize.X * SizeScaleX.Eval(T, 1.0f);
		Particle->Size.Y = Particle->BaseSize.Y * SizeScaleY.Eval(T, 1.0f);
		Particle->Size.Z = Particle->BaseSize.Z * SizeScaleZ.Eval(T, 1.0f);
	}
}

void UParticleModuleSize::GetCurves(TArray<FParticleCurveRef>& OutCurves)
{
	OutCurves.Add({ "Size Scale X", &SizeScaleX, { 1.0f, 0.3f, 0.3f, 1.0f }, 0 });
	OutCurves.Add({ "Size Scale Y", &SizeScaleY, { 0.3f, 1.0f, 0.3f, 1.0f }, 0 });
	OutCurves.Add({ "Size Scale Z", &SizeScaleZ, { 0.35f, 0.55f, 1.0f, 1.0f }, 0 });
}

void UParticleModuleSize::Serialize(FStructuredArchive::FRecord Record)
{
	// ObjectLabel + 리플렉션 프로퍼티(StartSize, bUseSizeOverLife, bEnabled)
	Super::Serialize(Record);

	if (TOptional<FStructuredArchive::FSlot> Slot = Record.TryEnterField("SizeScaleOverLife", true); Slot.IsSet())
	{
		FStructuredArchiveRecord Curves = Slot.GetValue().EnterRecord();
		SizeScaleX.Serialize(Curves.EnterField(FArchiveFieldName("X")));
		SizeScaleY.Serialize(Curves.EnterField(FArchiveFieldName("Y")));
		SizeScaleZ.Serialize(Curves.EnterField(FArchiveFieldName("Z")));
		return;
	}

	// 커브가 없는 예전 파일: BaseSize → EndSize 선형 보간을 0→1 배율 커브로 옮긴다.
	// 배율 기준은 StartSize 범위의 가운데 크기다. (가운데 크기로 스폰된 파티클은 예전과 똑같이 EndSize에서 끝난다)
	TOptional<FStructuredArchive::FSlot> EndSlot = Record.TryEnterField("EndSize", false);
	if (!EndSlot.IsSet())
		return;
	FVector EndSize(1.0f, 1.0f, 1.0f);
	EndSlot.GetValue() << EndSize;
	const auto ToScale = [](float End, float Min, float Max)
	{
		const float Mid = (Min + Max) * 0.5f;
		return std::abs(Mid) > 1e-6f ? End / Mid : 1.0f;
	};
	SizeScaleX = FInterpCurveFloat::MakeLinear(1.0f, ToScale(EndSize.X, StartSize.Min.X, StartSize.Max.X));
	SizeScaleY = FInterpCurveFloat::MakeLinear(1.0f, ToScale(EndSize.Y, StartSize.Min.Y, StartSize.Max.Y));
	SizeScaleZ = FInterpCurveFloat::MakeLinear(1.0f, ToScale(EndSize.Z, StartSize.Min.Z, StartSize.Max.Z));
}

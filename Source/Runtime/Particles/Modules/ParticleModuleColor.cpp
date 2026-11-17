#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleColor.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleHelper.h"

#include <cmath>

void UParticleModuleColor::Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle)
{
	const FLinearColor C = StartColor.Sample();
	Particle.Color = C;
	Particle.BaseColor = C;
}

void UParticleModuleColor::Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime)
{
	if (!bUseColorOverLife) return;

	for (int32 i = 0; i < Owner.ActiveParticles; ++i)
	{
		DECLARE_PARTICLE_PTR(Particle, Owner.ParticleData + Owner.ParticleStride * Owner.ParticleIndices[i]);
		if (Particle->Flags & STATE_Particle_Freeze) continue;

		const float T = std::clamp(Particle->RelativeTime, 0.0f, 1.0f);
		Particle->Color.R = Particle->BaseColor.R * ColorScaleR.Eval(T, 1.0f);
		Particle->Color.G = Particle->BaseColor.G * ColorScaleG.Eval(T, 1.0f);
		Particle->Color.B = Particle->BaseColor.B * ColorScaleB.Eval(T, 1.0f);
		Particle->Color.A = Particle->BaseColor.A * AlphaScale.Eval(T, 1.0f);
	}
}

void UParticleModuleColor::GetCurves(TArray<FParticleCurveRef>& OutCurves)
{
	OutCurves.Add({ "Color Scale R", &ColorScaleR, { 1.0f, 0.3f, 0.3f, 1.0f } });
	OutCurves.Add({ "Color Scale G", &ColorScaleG, { 0.3f, 1.0f, 0.3f, 1.0f } });
	OutCurves.Add({ "Color Scale B", &ColorScaleB, { 0.35f, 0.55f, 1.0f, 1.0f } });
	OutCurves.Add({ "Alpha Scale", &AlphaScale, { 0.85f, 0.85f, 0.85f, 1.0f } });
}

void UParticleModuleColor::SetFadeOut()
{
	ColorScaleR = FInterpCurveFloat::MakeLinear(1.0f, 1.0f);
	ColorScaleG = FInterpCurveFloat::MakeLinear(1.0f, 1.0f);
	ColorScaleB = FInterpCurveFloat::MakeLinear(1.0f, 1.0f);
	AlphaScale = FInterpCurveFloat::MakeLinear(1.0f, 0.0f);
}

void UParticleModuleColor::Serialize(FStructuredArchive::FRecord Record)
{
	// ObjectLabel + 리플렉션 프로퍼티(StartColor, bUseColorOverLife, bEnabled)
	Super::Serialize(Record);

	if (TOptional<FStructuredArchive::FSlot> Slot = Record.TryEnterField("ColorScaleOverLife", true); Slot.IsSet())
	{
		FStructuredArchiveRecord Curves = Slot.GetValue().EnterRecord();
		ColorScaleR.Serialize(Curves.EnterField(FArchiveFieldName("R")));
		ColorScaleG.Serialize(Curves.EnterField(FArchiveFieldName("G")));
		ColorScaleB.Serialize(Curves.EnterField(FArchiveFieldName("B")));
		AlphaScale.Serialize(Curves.EnterField(FArchiveFieldName("A")));
		return;
	}

	// 커브가 없는 예전 파일: BaseColor → EndColor 선형 보간을 0→1 배율 커브로 옮긴다.
	// 배율 기준은 StartColor 범위의 가운데 색이다. 기준이 0인 채널은 EndColor가 0이면 0, 아니면 1로 둔다.
	TOptional<FStructuredArchive::FSlot> EndSlot = Record.TryEnterField("EndColor", false);
	if (!EndSlot.IsSet())
		return;
	FLinearColor EndColor(1.0f, 1.0f, 1.0f, 0.0f);
	EndSlot.GetValue() << EndColor;
	const auto ToScale = [](float End, float Min, float Max)
	{
		const float Mid = (Min + Max) * 0.5f;
		if (std::abs(Mid) > 1e-6f)
			return End / Mid;
		return std::abs(End) > 1e-6f ? 1.0f : 0.0f;
	};
	ColorScaleR = FInterpCurveFloat::MakeLinear(1.0f, ToScale(EndColor.R, StartColor.Min.R, StartColor.Max.R));
	ColorScaleG = FInterpCurveFloat::MakeLinear(1.0f, ToScale(EndColor.G, StartColor.Min.G, StartColor.Max.G));
	ColorScaleB = FInterpCurveFloat::MakeLinear(1.0f, ToScale(EndColor.B, StartColor.Min.B, StartColor.Max.B));
	AlphaScale = FInterpCurveFloat::MakeLinear(1.0f, ToScale(EndColor.A, StartColor.Min.A, StartColor.Max.A));
}

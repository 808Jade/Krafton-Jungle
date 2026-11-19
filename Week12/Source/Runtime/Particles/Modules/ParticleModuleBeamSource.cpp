#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleBeamSource.h"

FVector UParticleModuleBeamSource::EvaluateDistribution(float EmitterTime) const
{
	switch (DistributionType)
	{
	case EBeamDistributionType::Uniform:
		return UniformRange.Sample();
	case EBeamDistributionType::ConstantCurve:
		return FVector(PositionX.Eval(EmitterTime, SourcePosition.X),
		    PositionY.Eval(EmitterTime, SourcePosition.Y),
		    PositionZ.Eval(EmitterTime, SourcePosition.Z));
	default:
		return SourcePosition;
	}
}

void UParticleModuleBeamSource::GetCurves(TArray<FParticleCurveRef>& OutCurves)
{
	if (DistributionType != EBeamDistributionType::ConstantCurve) return;
	OutCurves.Add({ "Source X (Emitter Time, seconds)", &PositionX, { 1.0f, 0.3f, 0.3f, 1.0f } });
	OutCurves.Add({ "Source Y (Emitter Time, seconds)", &PositionY, { 0.3f, 1.0f, 0.3f, 1.0f } });
	OutCurves.Add({ "Source Z (Emitter Time, seconds)", &PositionZ, { 0.3f, 0.5f, 1.0f, 1.0f } });
}

bool UParticleModuleBeamSource::IsPropertyVisibleInDetails(const FProperty& Property) const
{
	const FString Name = Property.Name.ToString();
	if (Name == "ParameterName")
		return Method == EBeamEndpointMethod::Actor || Method == EBeamEndpointMethod::Component;
	if (Name == "SourcePosition") return DistributionType == EBeamDistributionType::Constant;
	if (Name == "UniformRange") return DistributionType == EBeamDistributionType::Uniform;
	return Super::IsPropertyVisibleInDetails(Property);
}

void UParticleModuleBeamSource::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);
	if (auto Slot = Record.TryEnterField("PositionCurves", true); Slot.IsSet())
	{
		auto Curves = Slot.GetValue().EnterRecord();
		PositionX.Serialize(Curves.EnterField(FArchiveFieldName("X")));
		PositionY.Serialize(Curves.EnterField(FArchiveFieldName("Y")));
		PositionZ.Serialize(Curves.EnterField(FArchiveFieldName("Z")));
	}
}
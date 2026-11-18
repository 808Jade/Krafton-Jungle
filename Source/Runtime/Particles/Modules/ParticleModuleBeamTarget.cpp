#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleBeamTarget.h"

FVector UParticleModuleBeamTarget::EvaluateDistribution(float EmitterTime) const
{
	switch (DistributionType)
	{
	case EBeamDistributionType::Uniform:
		return UniformRange.Sample();
	case EBeamDistributionType::ConstantCurve:
		return FVector(PositionX.Eval(EmitterTime, TargetPosition.X),
		    PositionY.Eval(EmitterTime, TargetPosition.Y),
		    PositionZ.Eval(EmitterTime, TargetPosition.Z));
	default:
		return TargetPosition;
	}
}

void UParticleModuleBeamTarget::GetCurves(TArray<FParticleCurveRef>& OutCurves)
{
	if (DistributionType != EBeamDistributionType::ConstantCurve) return;
	OutCurves.Add({ "Target X (Emitter Time, seconds)", &PositionX, { 1.0f, 0.3f, 0.3f, 1.0f } });
	OutCurves.Add({ "Target Y (Emitter Time, seconds)", &PositionY, { 0.3f, 1.0f, 0.3f, 1.0f } });
	OutCurves.Add({ "Target Z (Emitter Time, seconds)", &PositionZ, { 0.3f, 0.5f, 1.0f, 1.0f } });
}

bool UParticleModuleBeamTarget::IsPropertyVisibleInDetails(const FProperty& Property) const
{
	const FString Name = Property.Name.ToString();
	if (Name == "ParameterName")
		return Method == EBeamEndpointMethod::Actor || Method == EBeamEndpointMethod::Component;
	if (Name == "TargetPosition") return DistributionType == EBeamDistributionType::Constant;
	if (Name == "UniformRange") return DistributionType == EBeamDistributionType::Uniform;
	return Super::IsPropertyVisibleInDetails(Property);
}

void UParticleModuleBeamTarget::Serialize(FStructuredArchive::FRecord Record)
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
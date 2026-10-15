#include "EnginePCH.h"
#include "LightSceneProxy.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"

void FDirectionalLightSceneProxy::GetLightShaderParameters(FLightRenderParameters& Out) const
{
	const UDirectionalLightComponent* Dir = static_cast<const UDirectionalLightComponent*>(LightComponent);

	FTransform Transform;
	Transform.Rotation = Dir->GetWorldRotation();
	const FLinearColor Color(Dir->GetLightColor());

	Out.Type = static_cast<uint32>(ELightType::Directional);
	Out.Direction = Transform.GetForward();
	Out.Color = FVector(Color.R, Color.G, Color.B);
	Out.Intensity = Dir->GetIntensity();
}

FDirectionalLightRenderData FDirectionalLightSceneProxy::GetRenderData() const
{
	FLightRenderParameters P{};
	GetLightShaderParameters(P);
	FDirectionalLightRenderData Data{};
	Data.DirectionIntensity = FVector4(P.Direction.X, P.Direction.Y, P.Direction.Z, P.Intensity);
	Data.Color = FVector4(P.Color.X, P.Color.Y, P.Color.Z, 1.0f);
	return Data;
}

void FPointLightSceneProxy::GetLightShaderParameters(FLightRenderParameters& Out) const
{
	const auto* PointLightComponent = static_cast<const UPointLightComponent*>(LightComponent);
	const FLinearColor Color(PointLightComponent->GetLightColor());

	Out = FLightRenderParameters{};
	Out.Type = static_cast<uint32>(ELightType::Point);
	Out.Position = PointLightComponent->GetWorldLocation();
	Out.Radius = PointLightComponent->GetAttenuationRadius();
	Out.Color = FVector(Color.R, Color.G, Color.B);
	Out.Intensity = PointLightComponent->GetIntensity();
	Out.FalloffExponent = PointLightComponent->GetFalloff();
}

FPointLightRenderData FPointLightSceneProxy::GetRenderData() const
{
	FLightRenderParameters P{};
	GetLightShaderParameters(P);
	FPointLightRenderData Data{};
	Data.PositionRadius = FVector4(P.Position.X, P.Position.Y, P.Position.Z, P.Radius);
	Data.ColorIntensity = FVector4(P.Color.X, P.Color.Y, P.Color.Z, P.Intensity);
	Data.Falloff = FVector4(P.FalloffExponent, 0.0f, 0.0f, 0.0f);
	return Data;
}

void FSpotLightSceneProxy::GetLightShaderParameters(FLightRenderParameters& Out) const
{
	const auto* SpotLightComponent = static_cast<const USpotLightComponent*>(LightComponent);
	const FLinearColor Color(SpotLightComponent->GetLightColor());

	FTransform Transform;
	Transform.Rotation = SpotLightComponent->GetWorldRotation();

	const float Outer = FMath::Clamp(SpotLightComponent->GetOuterConeAngle(), 0.0f, 89.0f);
	const float Inner = FMath::Clamp(SpotLightComponent->GetInnerConeAngle(), 0.0f, Outer);

	Out = FLightRenderParameters{};
	Out.Position = SpotLightComponent->GetWorldLocation();
	Out.Radius = SpotLightComponent->GetAttenuationRadius();
	Out.Color = FVector(Color.R, Color.G, Color.B);
	Out.Intensity = SpotLightComponent->GetIntensity();
	Out.FalloffExponent = SpotLightComponent->GetFalloff();

	Out.Type = static_cast<uint32>(ELightType::Spot);
	Out.Direction = Transform.GetForward();
	Out.InnerConeCos = std::cos(FMath::DegreesToRadians(Inner));
	Out.OuterConeCos = std::cos(FMath::DegreesToRadians(Outer));
}

FSpotLightRenderData FSpotLightSceneProxy::GetRenderData() const
{
	FLightRenderParameters P{};
	GetLightShaderParameters(P);
	FSpotLightRenderData Data{};
	Data.PositionRadius = FVector4(P.Position.X, P.Position.Y, P.Position.Z, P.Radius);
	Data.DirectionOuterCos = FVector4(P.Direction.X, P.Direction.Y, P.Direction.Z, P.OuterConeCos);
	Data.ColorIntensity = FVector4(P.Color.X, P.Color.Y, P.Color.Z, P.Intensity);
	Data.InnerCosFalloff = FVector4(P.InnerConeCos, P.FalloffExponent, 0.0f, 0.0f);
	return Data;
}

void FRectLightSceneProxy::GetLightShaderParameters(FLightRenderParameters& Out) const
{
	const auto* RectLightComponent = static_cast<const URectLightComponent*>(LightComponent);
	const FLinearColor Color(RectLightComponent->GetLightColor());

	FTransform Transform;
	Transform.Rotation = RectLightComponent->GetWorldRotation();

	Out = FLightRenderParameters{};
	Out.Type = static_cast<uint32>(ELightType::Rect);
	Out.Position = RectLightComponent->GetWorldLocation();
	Out.Radius = RectLightComponent->GetAttenuationRadius();
	Out.Color = FVector(Color.R, Color.G, Color.B);
	Out.Intensity = RectLightComponent->GetIntensity();
	Out.FalloffExponent = 1.0f;
	Out.Direction = Transform.GetForward();
	Out.Right = Transform.GetRight();
	Out.HalfWidth = RectLightComponent->GetWidth() * 0.5f;
	Out.HalfHeight = RectLightComponent->GetHeight() * 0.5f;

	const float AngleRad = FMath::DegreesToRadians(FMath::Clamp(RectLightComponent->GetBarnDoorAngle(), 0.0f, 90.0f));
	Out.BarnDoorCos = std::cos(AngleRad);
	Out.BarnDoorSin = std::sin(AngleRad);
	Out.BarnDoorLength = std::max(RectLightComponent->GetBarnDoorLength(), 0.0f);
}

FRectLightRenderData FRectLightSceneProxy::GetRenderData() const
{
	FLightRenderParameters P{};
	GetLightShaderParameters(P);
	FRectLightRenderData Data{};
	Data.PositionRadius = FVector4(P.Position.X, P.Position.Y, P.Position.Z, P.Radius);
	Data.DirectionBarnDoor = FVector4(P.Direction.X, P.Direction.Y, P.Direction.Z, P.BarnDoorLength);
	Data.ColorIntensity = FVector4(P.Color.X, P.Color.Y, P.Color.Z, P.Intensity);
	Data.Parameters = FVector4(P.BarnDoorCos, P.BarnDoorSin, P.FalloffExponent, 0.0f);
	Data.RectRightWidth = FVector4(P.Right.X, P.Right.Y, P.Right.Z, P.HalfWidth);
	Data.RectHeight = FVector4(0.0f, 0.0f, 0.0f, P.HalfHeight);
	return Data;
}

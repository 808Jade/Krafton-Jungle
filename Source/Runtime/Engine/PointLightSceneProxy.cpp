#include "EnginePCH.h"
#include "PointLightSceneProxy.h"

#include "Components/PointLightComponent.h"

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

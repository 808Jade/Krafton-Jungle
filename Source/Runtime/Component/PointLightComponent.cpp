#include "EnginePCH.h"
#include "PointLightComponent.h"
#include "Engine/LightSceneProxy.h"

void UPointLightComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);

	// 음수 반경은 감쇠 계산을 뒤집으므로 막는다.
	if (AttenuationRadius < 0.0f)
		AttenuationRadius = 0.0f;
}

FPointLightInfo UPointLightComponent::GetPointLightInfo() const
{
	FPointLightInfo Info;
	Info.Color = GetShaderColorWithIntensity();
	Info.AttenuationRadius = AttenuationRadius;
	Info.Position = GetWorldLocation();
	return Info;
}

FShadowInfo UPointLightComponent::GetShadowInfo() const
{
	FShadowInfo ShadowInfo;
	ShadowInfo.LightViewProjection = GetViewProjectionMatrix(false, AttenuationRadius, 90.f, 1.f);
	ShadowInfo.Bias = GetShadowBias();
	return ShadowInfo;
}


FLightSceneProxy* UPointLightComponent::CreateSceneProxy()
{
	return new FPointLightSceneProxy(this);
}

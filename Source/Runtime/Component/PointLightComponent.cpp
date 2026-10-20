#include "EnginePCH.h"
#include "PointLightComponent.h"
#include "Engine/LightSceneProxy.h"
#include "Render/LightInfo.h"

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


void UPointLightComponent::GetShadowInfo(TArray<FShadowInfo>& OutShadowInfo) const
{
	//FOV는 90도
	//ViewProjection이 달라서 월드 축 6방향으로 찍는다.
	OutShadowInfo.Reset();

	FShadowInfo ShadowInfo;
	ShadowInfo.Bias = GetShadowBias();

	//FRotator Pitch Yaw Roll 순서
	//Forward 0.f, 0.f, 0.f
	FRotator Rotator = FRotator(0.f, 0.f, 0.f);
	ShadowInfo.LightViewProjection = GetViewProjectionMatrix(Rotator, false, AttenuationRadius, 90.f, 1.f);
	OutShadowInfo.Add(ShadowInfo);

	//Back 0.f, 180.f, 0.f
	Rotator = FRotator(0.f, 180.f, 0.f);
	ShadowInfo.LightViewProjection = GetViewProjectionMatrix(Rotator, false, AttenuationRadius, 90.f, 1.f);
	OutShadowInfo.Add(ShadowInfo);

	//Right 0.f, 90.f, 0.f
	Rotator = FRotator(0.f, 90.f, 0.f);
	ShadowInfo.LightViewProjection = GetViewProjectionMatrix(Rotator, false, AttenuationRadius, 90.f, 1.f);
	OutShadowInfo.Add(ShadowInfo);

	//Left 0.f, -90.f, 0.f
	Rotator = FRotator(0.f, -90.f, 0.f);
	ShadowInfo.LightViewProjection = GetViewProjectionMatrix(Rotator, false, AttenuationRadius, 90.f, 1.f);
	OutShadowInfo.Add(ShadowInfo);

	//Up -90.f, 0.f, 0.f
	Rotator = FRotator(-90.f, 0.f, 0.f);
	ShadowInfo.LightViewProjection = GetViewProjectionMatrix(Rotator, false, AttenuationRadius, 90.f, 1.f);
	OutShadowInfo.Add(ShadowInfo);

	//Down 90.f, 0.f, 0.f
	Rotator = FRotator(90.f, 0.f, 0.f);
	ShadowInfo.LightViewProjection = GetViewProjectionMatrix(Rotator, false, AttenuationRadius, 90.f, 1.f);
	OutShadowInfo.Add(ShadowInfo);
}


FLightSceneProxy* UPointLightComponent::CreateSceneProxy()
{
	return new FPointLightSceneProxy(this);
}

#pragma once

#include "LightComponent.h"

struct FShadowInfo;

class UPointLightComponent : public ULightComponent
{
	DECLARE_CLASS(UPointLightComponent, ULightComponent)
	REFLECT_START(UPointLightComponent)
		PROPERTY(AttenuationRadius)
		PROPERTY(LightSourceRadius)
	REFLECT_END()

public:
	UPointLightComponent() = default;
	virtual ~UPointLightComponent() override = default;

	float GetAttenuationRadius() const { return AttenuationRadius; }

	FPointLightInfo GetPointLightInfo() const;
	virtual void GetShadowInfo(TArray<FShadowInfo>& OutShadowInfo) const override;
	virtual FLightSceneProxy* CreateSceneProxy() override;

	virtual void PostEditChangeProperty(const FProperty& Property) override;

protected:
	float AttenuationRadius = 10.0f;
	float LightSourceRadius = 0.03f;
};

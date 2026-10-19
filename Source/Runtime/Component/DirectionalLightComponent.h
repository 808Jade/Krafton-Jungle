#pragma once

#include "LightComponent.h"

class FLineBatcher;

class UDirectionalLightComponent : public ULightComponent
{
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)
	REFLECT_START(UDirectionalLightComponent)
		REFLECT_END()

public:
	UDirectionalLightComponent() = default;
	virtual ~UDirectionalLightComponent() override = default;

	FDirectionalLightInfo GetDirectionalLightInfo() const;
	virtual FShadowInfo GetShadowInfo() const override;
	virtual FLightSceneProxy* CreateSceneProxy() override;

	void DrawDebug(FLineBatcher* LineBatcher) const;

private:
};

#pragma once

#include "LightSceneProxy.h"
#include "Render/LightRenderData.h"

class FDirectionalLightSceneProxy : public FLightSceneProxy
{
public:
	using FLightSceneProxy::FLightSceneProxy;
	void GetLightShaderParameters(FLightRenderParameters& Out) const override;
	FDirectionalLightRenderData GetRenderData() const;
};
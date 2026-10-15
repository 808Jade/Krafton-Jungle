#pragma once

#include "LocalLightSceneProxy.h"
#include "Render/LightRenderData.h"

class FPointLightSceneProxy : public FLocalLightSceneProxy
{
public:
    using FLocalLightSceneProxy::FLocalLightSceneProxy;
    void GetLightShaderParameters(FLightRenderParameters& Out) const override;
    FPointLightRenderData GetRenderData() const;
};
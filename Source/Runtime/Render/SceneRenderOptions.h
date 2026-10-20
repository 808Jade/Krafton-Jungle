#pragma once

#include "Render/RenderEnums.h"

// 뷰 하나의 씬 렌더링 옵션. 에디터와 게임 뷰포트가 같은 설정을 전달한다.
struct FSceneRenderOptions
{
	bool bDrawPrimitives = true;
	bool bShowShadow = true;
	float ShadowDepthBias = 0.0001f;
	bool bEnableFog = true;
	bool bEnableFXAA = false;
	ELightCullingMode LightCullingMode = ELightCullingMode::Tiled;
	bool bShowSceneDepth = false;
	bool bShowWorldNormal = false;
	bool bShowLightCulling = false;
	float SceneDepthRange = 1.0f;
	uint32 DirectionalShadowMapSize = 0.f;
	uint32 PointShadowMapSize = 0.f;
	uint32 SpotShadowMapSize = 0.f;

	ERenderingPath RenderingPath = ERenderingPath::Deferred;
	ELightingModel LightingModel = ELightingModel::Gouraud;
};

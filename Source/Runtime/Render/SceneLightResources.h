#pragma once

#include "Render/LightRenderData.h"
#include "Render/ForwardLightTiles.h"
#include "Render/Buffer.h"

struct FShadowDrawPass;
class FRenderer;

struct FDeferredLightConstants
{
	FMatrix InverseViewProjection;
	FVector4 CameraPosition;
	FVector4 CameraForward;
	FVector4 ViewportSize;
	FAmbientLightRenderData Ambient;
	uint32 LightCounts[4];
	uint32 LightOffsets[4];
	FVector4 SkyColorIntensity;
};

// 뷰별 조명 GPU 리소스. 패스나 드로우는 실행하지 않는다.
class FSceneLightResources
{
public:
	static constexpr uint32 MaxShadowAtlases = 4; // DeferredLightingShader.hlsl과 동일
	bool Init();
	// 광원을 종류별 구간(방향광, 포인트, 스포트, 렉트 순)으로 정렬해 업로드한다.
	// Counts/Offsets 순서는 x: 포인트, y: 스포트, z: 방향광, w: 렉트다.
	bool UploadViewLights(const TArray<FLightRenderParameters>& Lights,
		uint32 (&Counts)[4], uint32 (&Offsets)[4]);
	// Atlas 배정 후 CPU 업로드 목록만 구성한다. GPU 업로드는 하지 않는다.
	bool BuildViewShadowParameters(const TArray<FShadowDrawPass>& Passes);
	bool UploadViewShadows(bool bEnabled, uint32 AtlasCount, float DepthBias);

	const TArray<FShadowParameters>& GetShadowParameters() const { return ShadowParameters; }
	const TArray<FLightShadowRange>& GetLightShadowRanges() const { return LightShadowRanges; }

	FStructuredBuffer* GetSceneLightBuffer() const { return SceneLightBuffer.get(); }
	FConstantBuffer* GetForwardConstants() const { return ForwardLightCB.get(); }
	FConstantBuffer* GetDeferredConstants() const { return DeferredLightCB.get(); }
	FConstantBuffer* GetShadowConstants() const { return ShadowCB.get(); }
	FStructuredBuffer* GetShadowParameterBuffer() const { return bShadowsUploaded == true ? ShadowParameterBuffer.get() : nullptr ; }
	FStructuredBuffer* GetShadowRangeBuffer() const { return bShadowsUploaded == true ? LightShadowRangeBuffer.get() : nullptr ; }


	FForwardLightTileGrid& GetTiles() { return ForwardTileGrid; }

private:
	TUniquePtr<FStructuredBuffer> SceneLightBuffer;
	TUniquePtr<FConstantBuffer> ForwardLightCB;
	TUniquePtr<FConstantBuffer> DeferredLightCB;
	TUniquePtr<FStructuredBuffer> ShadowParameterBuffer;
	TUniquePtr<FStructuredBuffer> LightShadowRangeBuffer;
	TUniquePtr<FConstantBuffer> ShadowCB;
	TArray<uint32> PackedLightIndices; // VisibleLights 인덱스 -> SceneLights 인덱스
	uint32 PackedLightCount = 0;
	TArray<FShadowParameters> ShadowParameters;
	TArray<FLightShadowRange> LightShadowRanges;
	bool bShadowsUploaded = false;
	FForwardLightTileGrid ForwardTileGrid;
	bool bInitialized = false;
};

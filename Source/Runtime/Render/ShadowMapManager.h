#pragma once

#include "Render/LightInfo.h"
#include "Containers/Array.h"
#include "Buffer.h"

class FLightSceneProxy;

class FShadowMapManager
{
public:
	uint32 GetNumShadowInfos() const { return NumShadowInfos; }

	void UploadShadowInfos(const TArray<FShadowInfo>& ShadowInfos);

	// 섀도 패스
	ID3D11DepthStencilView* GetSliceDSV(uint32 Slice) const;
	ID3D11ShaderResourceView* GetShadowMapArraySRV() const;
	ID3D11ShaderResourceView* GetShadowInfoSRV() const;
	uint32 GetResolution() const { return Resolution; }
	uint32 GetNumSlices() const { return NumSlices; }
	uint32 GetDirectionalResolution() const { return DirectionalResolution; }
	ID3D11ShaderResourceView* GetDirectionalShadowSRV() const { return DirectionalShadowSRV.Get(); }
		
	//그리기 전 바인딩
	void BeginShadowRender(uint32 Slice);
	//그리고 나서 해제
	void EndShadowPass();

	//바인딩 해제
	void UnbindForShading();
	//그리고 나서 바인딩
	void BindForShading();

	void ComputeCascadeSpheres(
		const FVector& CameraPosition,
		const FVector& CameraForward,
		float FovDegrees,
		float AspectRatio,
		float NearClip,
		float ShadowMaxDist,
		FVector4 OutCascadeSpheres[4]);
	void ComputeCascadeLightMatrices(
		const FVector& LightDirection,
		const FVector4 InCascadeSpheres[4],
		uint32 CascadeResolution,
		FMatrix OutCascadeMatrices[4]);
	void SetupDirectionalShadow(
		const FShadowInfo& DirectionalShadow,
		const FVector& LightDirection,
		const FVector& CameraPosition,
		const FVector& CameraForward,
		float FovDegrees,
		float AspectRatio);
	void BeginDirectionalCascadeRender(uint32 CascadeIndex);
	void BeginDirectionalShadowRender();
	void EndDirectionalShadowRender();

	const FMatrix& GetCascadeMatrix(uint32 CascadeIndex) const { return CachedCascadeMatrices[CascadeIndex]; }

	//Imgui
	void UpdatePreview(FLightSceneProxy* PreviewShadowMapProxy);

private:
	// 해상도, 층 수가 바뀔 때만 리소스와 뷰를 다시 만든다
	bool EnsureShadowMaps(uint32 InResolution, uint32 InNumSlices);
	bool EnsureShadowInfoCapacity(uint32 RequiredCount);	
	bool EnsureDirectionalShadowMap(uint32 InResolution);

	ComPtr<ID3D11Buffer> ShadowInfoBuffer;
	ComPtr<ID3D11ShaderResourceView> ShadowInfoSRV;

	ComPtr<ID3D11Texture2D> ShadowMapArrayBuffer;
	ComPtr<ID3D11ShaderResourceView> ShadowMapArraySRV;
	TArray<ComPtr<ID3D11DepthStencilView>> ShadowDepthView;
	
	ComPtr<ID3D11Texture2D> DirectionalShadowTexture;
	ComPtr<ID3D11ShaderResourceView> DirectionalShadowSRV;
	ComPtr<ID3D11DepthStencilView> DirectionalShadowDSV;
	TUniquePtr<FConstantBuffer> DirectionalShadowCB;
	FMatrix CachedCascadeMatrices[4] = {};
	uint32 DirectionalResolution = 4096;

	uint32 ShadowInfoCapacity = 0;
	uint32 NumShadowInfos = 0;	

	uint32 ShadowMapCapacity = 0;
	uint32 NumShadowMaps = 0;

	uint32 Resolution = 0;
	uint32 NumSlices = 0;
};

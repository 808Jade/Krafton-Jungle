#pragma once

#include "Render/LightInfo.h"
#include "Containers/Array.h"

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

	uint32 GetResolution() { return Resolution; }
	uint32 GetNumSlices() { return NumSlices; }

		
	//그리기 전 바인딩
	void BeginShadowRender(uint32 Slice);

	void ClearAtlas();

	//그리고 나서 해제
	void EndShadowPass();

	//바인딩 해제
	void UnbindForShading();
	//그리고 나서 바인딩
	void BindForShading();

	//Imgui
	void UpdatePreview(FLightSceneProxy* PreviewShadowMapProxy, const TArray<FShadowInfo>& ShadowInfos);

	// Atlas
	void BuildAtlas(TArray<FShadowInfo>& ShadowInfos);

	void BeginShadowRenderWithAtlas(const FShadowInfo& ShadowInfo);

	uint32 GetAtlasResolution() { return AtlasResolution; }
	uint32 GetNumAtlas() { return NumAtlas; }
	ID3D11ShaderResourceView* GetPreviewSRV() const { return PreviewShadowSRV.Get(); }

private:
	// 해상도, 층 수가 바뀔 때만 리소스와 뷰를 다시 만든다
	bool EnsureShadowMaps(uint32 InResolution, uint32 InNumSlices);
	
	bool EnsureShadowInfoCapacity(uint32 RequiredCount);	

	// 아틀라스 섀도우 맵 만들기
	bool EnsureShadowMapAtlas(uint32 InResolution, uint32 InNumSlices);

	ComPtr<ID3D11Buffer> ShadowInfoBuffer;
	ComPtr<ID3D11ShaderResourceView> ShadowInfoSRV;

	ComPtr<ID3D11Texture2D> ShadowMapArrayBuffer;
	ComPtr<ID3D11ShaderResourceView> ShadowMapArraySRV;
	TArray<ComPtr<ID3D11DepthStencilView>> ShadowDepthView;

	ComPtr<ID3D11Texture2D> ShadowMapAtlasBuffer;
	ComPtr<ID3D11ShaderResourceView> ShadowMapAtlasArraySRV;
	TArray<ComPtr<ID3D11DepthStencilView>> ShadowAtlasDepthView;

	uint32 ShadowInfoCapacity = 0;
	uint32 NumShadowInfos = 0;	

	uint32 ShadowMapCapacity = 0;
	uint32 NumShadowMaps = 0;

	uint32 Resolution = 512;
	uint32 NumSlices = 0;

	uint32 AtlasResolution = 4096;
	uint32 NumAtlas = 1;

	// 프리뷰는 섀도우 매니저가 하나만 들고 있는다.
	ComPtr<ID3D11Texture2D> PreviewShadowTexture;
	ComPtr<ID3D11ShaderResourceView> PreviewShadowSRV;
};

#pragma once

#include <d3d11.h>
#include "Asset/RenderAsset.h"

class UTexture2D : public URenderAsset
{
	DECLARE_CLASS(UTexture2D, URenderAsset)

	friend class FRenderer;
public:
	UTexture2D();
	~UTexture2D();

	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }
	ID3D11Texture2D* GetTexture() { return Texture.Get(); }
private:
	uint32 Width;
	uint32 Height;

	ComPtr<ID3D11Texture2D> Texture;
	ComPtr<ID3D11ShaderResourceView> ShaderResourceView;
	ComPtr<ID3D11RenderTargetView> RenderTargetView;
};
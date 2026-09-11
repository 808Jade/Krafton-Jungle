#pragma once

#include <d3d11.h>
#include "Asset/RenderAsset.h"

class UTexture2D : public URenderAsset
{
public:
	UTexture2D(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& Desc);
	~UTexture2D();

	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }
	ID3D11Texture2D* GetTexture() { return Texture2D.Get(); }
private:
	uint32 Width;
	uint32 Height;

	ComPtr<ID3D11Texture2D> Texture2D;
};
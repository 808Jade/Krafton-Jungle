#pragma once

#include <d3d11.h>
#include "Asset/RenderAsset.h"

class UTexture2D : public URenderAsset
{
	DECLARE_CLASS(UTexture2D, URenderAsset)

	friend class FRenderer;
public:
	// UObject 상속 때문에 인자로 받기 ㄴㄴ
	UTexture2D();
	~UTexture2D();

	uint32 GetWidth() const { return Desc.Width; }
	uint32 GetHeight() const { return Desc.Height; }

	ID3D11Resource* GetRawPtr() const { return Texture.Get(); }
	ID3D11ShaderResourceView* GetSRV() const { return SRV.Get(); }
	ID3D11RenderTargetView* GetRTV() const { return RTV.Get(); }

	static UTexture2D* CreateFromFile(const FString& FilePath);
	static UTexture2D* CreateFromEmpty(const FString& FilePath);
private:
	D3D11_TEXTURE2D_DESC Desc;

	ComPtr<ID3D11Resource> Texture;
	ComPtr<ID3D11ShaderResourceView> SRV;
	ComPtr<ID3D11RenderTargetView> RTV;
};
#pragma once

#include <d3d11.h>
#include "Asset/RenderAsset.h"
#include "Render/RenderDevice.h"

class FTexture2D
{
public:
	FTexture2D(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const void* InitialData = nullptr);
	// For Swapchain
	FTexture2D(ID3D11Device* Device, ComPtr<ID3D11Resource> SwapchainTexture, const D3D11_TEXTURE2D_DESC& InDesc);
	~FTexture2D() = default;

	uint32 GetWidth() const { return Desc.Width; }
	uint32 GetHeight() const { return Desc.Height; }

	ID3D11Resource* GetRawPtr() const { return Texture.Get(); }
	ID3D11ShaderResourceView* GetSRV() const { return SRV.Get(); }
	ID3D11RenderTargetView* GetRTV() const { return RTV.Get(); }
	ID3D11DepthStencilView* GetDSV() const { return DSV.Get(); }

private:
	void CreateViews(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& Desc);

	D3D11_TEXTURE2D_DESC Desc;

	ComPtr<ID3D11Resource> Texture;
	ComPtr<ID3D11ShaderResourceView> SRV;
	ComPtr<ID3D11RenderTargetView> RTV;
	ComPtr<ID3D11DepthStencilView> DSV;
};

class UTexture2D : public URenderAsset
{
	DECLARE_CLASS(UTexture2D, URenderAsset)

	friend class UAssetManager;
public:
	// UObject 상속 때문에 인자로 받기 ㄴㄴ
	UTexture2D();
	~UTexture2D();

	uint32 GetWidth()  const { return Texture ? Texture->GetWidth() : 0; }
	uint32 GetHeight() const { return Texture ? Texture->GetHeight() : 0; }

	FTexture2D* GetResource() const { return Texture.get(); }
private:
	void SetResource(TUniquePtr<FTexture2D> InResource) { Texture = std::move(InResource); }

	TUniquePtr<FTexture2D> Texture;
};


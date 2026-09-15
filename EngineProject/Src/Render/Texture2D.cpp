#include "EnginePCH.h"
#include "Texture2D.h"

#include "RenderCommand.h"

FTexture2D::FTexture2D(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const void* InitialData)
{
	Desc = InDesc;


	D3D11_SUBRESOURCE_DATA SubData{};
	if (InitialData)
	{
		SubData.pSysMem = InitialData;
		SubData.SysMemPitch = Desc.Width * 4;
	}

	HRESULT hr = Device->CreateTexture2D(&Desc, InitialData ? &SubData : nullptr, (ID3D11Texture2D**)Texture.GetAddressOf());
	if (FAILED(hr))
	{

	}

	CreateViews(Device, Desc);
}

FTexture2D::FTexture2D(ID3D11Device* Device, ComPtr<ID3D11Resource> SwapchainTexture, const D3D11_TEXTURE2D_DESC& InDesc)
{
	Texture = std::move(SwapchainTexture);
	Desc = InDesc;

	CreateViews(Device, Desc);
}

void FTexture2D::CreateViews(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& Desc)
{
	HRESULT hr;
	if (Desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		hr = Device->CreateShaderResourceView(Texture.Get(), nullptr, SRV.GetAddressOf());
		if (FAILED(hr))
		{
		}
	}

	if (Desc.BindFlags & D3D11_BIND_RENDER_TARGET)
	{
		hr = Device->CreateRenderTargetView(Texture.Get(), nullptr, RTV.GetAddressOf());
		if (FAILED(hr))
		{
		}
	}

	if (Desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
	{
		hr = Device->CreateDepthStencilView(Texture.Get(), nullptr, DSV.GetAddressOf());
		if (FAILED(hr))
		{
		}
	}
}

UTexture2D::UTexture2D()
{

}
UTexture2D::~UTexture2D()
{
}


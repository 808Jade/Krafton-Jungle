#include "EnginePCH.h"
#include "Texture2D.h"

FTexture2D::FTexture2D(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& Desc)
{
	Width = Desc.Width;
	Height = Desc.Height;
	Device->CreateTexture2D(&Desc, nullptr, Texture2D.GetAddressOf());
}
FTexture2D::~FTexture2D()
{
}

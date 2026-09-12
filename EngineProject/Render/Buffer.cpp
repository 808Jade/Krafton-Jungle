#include "EnginePCH.h"
#include "Buffer.h"

FBuffer::FBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc)
{
	BufferSize = Desc.ByteWidth;
	HRESULT hr = Device->CreateBuffer(&Desc, nullptr, Buffer.GetAddressOf());
	if (FAILED(hr))
	{
		LOG(Warning, "Failed To Create Buffer!");
	}
}

FVertexBuffer::FVertexBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc, uint32 InStride)
	:FBuffer(Device, Desc)
{
	Stride = InStride;

}


FIndexBuffer::FIndexBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc, uint32 Count)
	:FBuffer(Device, Desc)
{
	IndexCount = Count;
}

FConstantBuffer::FConstantBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc)
	:FBuffer(Device, Desc)
{
}




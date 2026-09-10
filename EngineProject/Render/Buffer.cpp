#include "EnginePCH.h"
#include "Buffer.h"

FIndexBuffer::FIndexBuffer(ID3D11Device* Device, const uint32* Indices, uint32 Count)
{
	IndexCount = Count;

	// Create a index buffer
	D3D11_BUFFER_DESC Desc = {};
	Desc.ByteWidth = sizeof(uint32) * Count;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA Data;
	Data.pSysMem = Indices;

	HRESULT hr = Device->CreateBuffer(&Desc, &Data, Buffer.GetAddressOf());
}

FVertexBuffer::FVertexBuffer(ID3D11Device* Device, const void* Vertices, uint32 TotalSize, uint32 InStride)
{
	Stride = InStride;

	// Create a index buffer
	D3D11_BUFFER_DESC Desc = {};
	Desc.ByteWidth = TotalSize;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA Data;
	Data.pSysMem = Vertices;

	HRESULT hr = Device->CreateBuffer(&Desc, &Data, Buffer.GetAddressOf());
}

FConstantBuffer::FConstantBuffer(ID3D11Device* Device, uint32 Size)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.ByteWidth = Size;
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = Device->CreateBuffer(&Desc, nullptr, Buffer.GetAddressOf());
}

void FConstantBuffer::UpdateData(const void* InData)
{
	
}

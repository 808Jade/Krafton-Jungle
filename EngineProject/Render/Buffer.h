#pragma once

#include <d3d11.h>
#include <wrl.h>

using namespace Microsoft::WRL;

class FIndexBuffer
{
public:
	FIndexBuffer(ID3D11Device* Device, const uint32* Indices, uint32 IndexCount);
	~FIndexBuffer() = default;

	inline uint32 GetIndexCount() const { return IndexCount; }
	inline ID3D11Buffer* GetBuffer() const { return Buffer.Get(); }

private:
	uint32 IndexCount;
	ComPtr<ID3D11Buffer> Buffer;

};

class FVertexBuffer
{
public:
	FVertexBuffer(ID3D11Device* Device, const void* Vertices, uint32 TotalSize, uint32 InStride);
	~FVertexBuffer() = default;

	inline uint32 GetStride() const { return Stride; }
	inline ID3D11Buffer* GetBuffer() const { return Buffer.Get(); }

private:
	uint32 Stride = 0;
	ComPtr<ID3D11Buffer> Buffer;

};

class FConstantBuffer
{
public:
	FConstantBuffer(ID3D11Device* Device, uint32 Size);
	~FConstantBuffer() = default;

	void UpdateData(const void* InData);
	inline ID3D11Buffer* GetBuffer() const { return Buffer.Get(); }

private:
	ComPtr<ID3D11Buffer> Buffer;
	void* DataPtr;
	uint32 Size;

};
#pragma once
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include "RenderPacket.h"
#include "Render/Buffer.h"

enum EShaderBindFlagBits : uint32
{
	None = 0,
	Vertex = 1,
	Geometry = 1 << 2,
	Domain = 1 << 3,
	Hull = 1 << 4,
	Pixel = 1 << 5,
	Compute = 1 << 6
};

class FRenderer
{
public:
	void BeginFrame();
	void EndFrame();

	bool Init(HWND hWindow);

	void CreateDeviceAndSwapChain(HWND hWindow);
	void CreateFrameBuffer();
	void CreateRasterizerState();
	void CreateDepthStencilBufferAndState();
	void CreateConstantBuffer();

	inline ID3D11Device* GetDevice() const { return Device.Get(); }
	ID3D11DeviceContext* GetDeviceContext();

	TSharedPtr<FShader> CreateShader(const wchar_t* FileName, D3D11_INPUT_ELEMENT_DESC* InLayoutDesc, size_t InLayoutSize);
	TSharedPtr<FShader> CreateShader(const wchar_t* FileName, std::vector<D3D11_INPUT_ELEMENT_DESC> Layouts);
	TSharedPtr<FMesh> CreateMesh(const FMeshData& InMeshData);
	TSharedPtr<FMesh> CreateMesh(TSharedPtr<FVertexBuffer> VertexBuffer, TSharedPtr<FIndexBuffer> IndexBuffer);

	TSharedPtr<FVertexBuffer> CreateVertexBuffer(const void* InVertices, uint32 InSize, uint32 Stride);
	TSharedPtr<FIndexBuffer> CreateIndexBuffer(const uint32* InIndices, uint32 IndexCount);
	TSharedPtr<FConstantBuffer> CreateConstantBuffer(uint32 BufferSize);

	void UpdateConstantBufferData(FConstantBuffer* Buffer, const void* Data, uint32 DataSize);

	void UpdateConstantBuffer(const FMatrix& MVP);
	void BindVertexBuffer(FVertexBuffer* VertexBuffer);
	void BindIndexBuffer(FIndexBuffer* IndexBuffer);
	void BindConstantBuffer(uint32 Slot, FConstantBuffer* ConstantBuffer, EShaderBindFlagBits FlagBits = None);
	void SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology);
	void SetDepthStencilEnabled(bool bEnabled);
	void BindShader(FShader* InShader);
	void BindMesh(FMesh* InMesh);

	void Draw(uint32 VertexCount);
	void DrawIndexed(uint32 IndexCount);

	// 윈도우 사이즈 변경
	void Resize(int32 Width, int32 Height);

	/*void Prepare();*/

	void RenderAll(TQueue<FRenderPacket>& InQueue, FMatrix VP);
	void Shutdown();

private:
	// Camera 
	struct FConstants
	{
		FMatrix MVP;
	};

	Microsoft::WRL::ComPtr<ID3D11Device> Device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> DeviceContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain> SwapChain;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> FrameBuffer;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> FrameBufferRTV;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> DepthStencilBuffer;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> FrameBufferDSV;

	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DepthStencilState;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DepthDisabledState;

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> RasterizerState;

	Microsoft::WRL::ComPtr <ID3D11Buffer> ConstantBuffer;
	D3D11_VIEWPORT ViewportInfo;

	uint32 Width;
	uint32 Height;

	FLOAT ClearColor[4] = { 0.3f, 0.3f, 0.3f, 1.0f };
};

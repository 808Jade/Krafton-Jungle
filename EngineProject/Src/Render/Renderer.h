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
#include "Texture2D.h"
#include "Text/FontAtlas.h"

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

	void CreateAlphaBlendState();

	inline ID3D11Device* GetDevice() const { return Device.Get(); }
	inline ID3D11DeviceContext* GetDeviceContext() const { return DeviceContext.Get(); }

	TSharedPtr<FFontAtlas> LoadFontAtlas(const FString& JsonPath, const FString& AtlasTexturePath);

	TSharedPtr<FShader> CreateShader(const wchar_t* FileName, D3D11_INPUT_ELEMENT_DESC* InLayoutDesc, size_t InLayoutSize);
	TSharedPtr<FShader> CreateShader(const wchar_t* FileName, std::vector<D3D11_INPUT_ELEMENT_DESC> Layouts);
	UStaticMesh* CreateMesh(const FStaticMeshData& InMeshData);
	UStaticMesh* CreateMesh(TSharedPtr<FVertexBuffer> VertexBuffer, TSharedPtr<FIndexBuffer> IndexBuffer);

	UTexture2D* CreateShaderResource(const FString& InPath);
	UTexture2D* CreateRenderTarget(const FString& InPath);

	TSharedPtr<FVertexBuffer> CreateVertexBuffer(const void* InVertices, uint32 InSize, uint32 Stride);
	TSharedPtr<FIndexBuffer> CreateIndexBuffer(const uint32* InIndices, uint32 MaxIndexCount);

	TSharedPtr<FVertexBuffer> CreateDynamicVertexBuffer(uint32 MaxSize, uint32 Stride);
	TSharedPtr<FIndexBuffer> CreateDynamicIndexBuffer(uint32 MaxIndexCount);

	TSharedPtr<FConstantBuffer> CreateConstantBuffer(uint32 BufferSize);

	void UpdateVertexBufferData(FVertexBuffer* InBuffer, const void* Data, uint32 DataSize);
	void UpdateIndexBufferData(FIndexBuffer* InBuffer, const void* Data, uint32 DataSize);
	void UpdateConstantBufferData(FConstantBuffer* InBuffer, const void* Data, uint32 DataSize);

	void UpdateConstantBuffer(const FMatrix& MVP);
	void BindVertexBuffer(FVertexBuffer* VertexBuffer);
	void BindIndexBuffer(FIndexBuffer* IndexBuffer);
	void BindConstantBuffer(uint32 Slot, FConstantBuffer* ConstantBuffer, EShaderBindFlagBits FlagBits);
	void BindTexture(uint32 Slot, UTexture2D* Texture2D, EShaderBindFlagBits FlagBits);
	void SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology);
	void SetDepthStencilEnabled(bool bEnabled);
	void SetBlendStateEnabled(bool bEnabled);
	void BindShader(FShader* InShader);
	void BindMesh(UStaticMesh* InMesh);

	// Todo: subuv
	void UpdateSubUVConstantBuffer(const FRenderPacket& Packet);

	void Draw(uint32 VertexCount, uint32 StartIndexLocation = 0);
	void DrawIndexed(uint32 IndexCount, uint32 StartIndexLocation = 0, int32 BaseVertexLocation = 0);

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

	struct FSubUVConstants
	{
		float CurrentFrame;
		float AtlasRowSize;
		float AtlasColSize;
		float Padding = 0.0f;
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
	Microsoft::WRL::ComPtr <ID3D11SamplerState> DefaultSamplerState;

	// Todo: subuv
	Microsoft::WRL::ComPtr<ID3D11BlendState> AlphaBlendState;
	Microsoft::WRL::ComPtr<ID3D11Buffer> SubUVConstantBuffer;

	D3D11_VIEWPORT ViewportInfo;

	uint32 Width;
	uint32 Height;

	FLOAT ClearColor[4] = { 0.3f, 0.3f, 0.3f, 1.0f };
};

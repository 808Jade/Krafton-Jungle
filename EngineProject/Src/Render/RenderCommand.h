#pragma once

#include "Renderer.h"

class RenderCommand
{
public:
	inline static void Init(FRenderer* InRenderer) { Renderer = InRenderer; };

	static UTexture2D* CreateTexture2DFromFile(const FString& FilePath);
	static UTexture2D* CreateRenderTargetTexture2D(uint32 Width, uint32 Height, DXGI_FORMAT Format);
	static UTexture2D* CreateDepthStencilTexture2D(uint32 Width, uint32 Height, DXGI_FORMAT Format);

	static void BindPipelineState() {};

	static void BindMesh(UStaticMesh* Mesh) {};

	static void Draw(uint32 VertexCount, uint32 StartIndexLocation = 0);
	static void DrawIndexed(uint32 IndexCount, uint32 StartIndexLocation = 0, int32 BaseVertexLocation = 0);


	//inline static void UpdateVertexBufferData(FVertexBuffer* InBuffer, const void* Data, uint32 DataSize);

	//inline static void UpdateIndexBufferData(FIndexBuffer* InBuffer, const void* Data, uint32 DataSize);

	//inline static void UpdateConstantBufferData(FConstantBuffer* InBuffer, const void* Data, uint32 DataSize);

	//inline static void UpdateConstantBuffer(const FMatrix& MVP);

	//inline static void BindVertexBuffer(FVertexBuffer* VertexBuffer);

	//inline static void BindIndexBuffer(FIndexBuffer* IndexBuffer);

	//inline static void BindConstantBuffer(uint32 Slot, FConstantBuffer* ConstantBuffer, EShaderBindFlagBits FlagBits);

	//inline static void BindTexture(uint32 Slot, UTexture2D* Texture2D, EShaderBindFlagBits FlagBits);

	//inline static void SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology);

	//inline static void SetDepthStencilEnabled(bool bEnabled);

	//inline static void SetBlendStateEnabled(bool bEnabled);

	//inline static void BindShader(FShader* InShader);

	//inline static void BindMesh(UStaticMesh* InMesh);


private:
	inline static FRenderer* Renderer = nullptr;
};
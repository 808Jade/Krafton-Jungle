#include "EnginePCH.h"
#include "RenderCommand.h"

UTexture2D* RenderCommand::CreateTexture2DFromFile(const FString& FilePath)
{
	return Renderer->CreateTexture2DFromFile(FilePath);
}

UTexture2D* RenderCommand::CreateRenderTargetTexture2D(uint32 Width, uint32 Height, DXGI_FORMAT Format)
{
	return Renderer->CreateRenderTargetTexture2D(Width, Height, Format);
}

inline void RenderCommand::Draw(uint32 VertexCount, uint32 StartIndexLocation)
{
	Renderer->Draw(VertexCount, StartIndexLocation);
}

inline void RenderCommand::DrawIndexed(uint32 IndexCount, uint32 StartIndexLocation, int32 BaseVertexLocation)
{
	Renderer->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
}

#include "EnginePCH.h"
#include "Renderer.h"
#include "Shader.h"
#include "Mesh.h"

#include "RenderCommand.h"

bool FRenderer::Init()
{
	CB = RenderCommand::CreateConstantBuffer(sizeof(FSubUVConstants));
	Temp = RenderCommand::CreateConstantBuffer(sizeof(FMatrix));

	return true;
}




UStaticMesh* FRenderer::CreateMesh(const FStaticMeshData& InMeshData)
{
	UStaticMesh* Mesh = FObjectFactory::ConstructObject<UStaticMesh>();

	Mesh->VertexBuffer = RenderCommand::CreateStaticVertexBuffer(InMeshData.Vertices.GetData(), sizeof(FVertex) * InMeshData.Vertices.Num(), sizeof(FVertex));
	Mesh->IndexBuffer = RenderCommand::CreateStaticIndexBuffer(InMeshData.Indices.GetData(), InMeshData.Indices.Num());

	return Mesh;
}


UStaticMesh* FRenderer::CreateMesh(TUniquePtr<FVertexBuffer> VertexBuffer, TUniquePtr<FIndexBuffer> IndexBuffer)
{
	UStaticMesh* Mesh = FObjectFactory::ConstructObject<UStaticMesh>();

	Mesh->VertexBuffer = std::move(VertexBuffer);
	Mesh->IndexBuffer = std::move(IndexBuffer);

	return Mesh;
}


//void FRenderer::Prepare()
//{
//	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//	DeviceContext->OMSetRenderTargets(1, FrameBufferRTV.GetAddressOf(), nullptr);
//	DeviceContext->RSSetViewports(1, &ViewportInfo);
//	DeviceContext->RSSetState(RasterizerState.Get());
//	DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffff'ffff);
//
//
//}

// Todo: subuv, Fix function

void FRenderer::RenderAll(TQueue<FRenderPacket>& InQueue, FMatrix VP)
{
	// Todo: subuv
	TArray<FRenderPacket> RenderPackets;

	while (InQueue.IsEmpty() == false)
	{
		RenderPackets.Add(InQueue.Peek());
		InQueue.Dequeue();
	}

	std::stable_sort(
		RenderPackets.begin(),
		RenderPackets.end(),
		[](const FRenderPacket& First, const FRenderPacket& Second) -> bool
		{
			if (First.bSubUV && Second.bSubUV)
			{
				return First.CameraToParticleDistance > Second.CameraToParticleDistance;
			}

			if (First.bSubUV == false || Second.bSubUV == false)
			{
				return true;
			}

			return false;
		});

	for (const FRenderPacket& RenderPacket : RenderPackets)
	{
		RenderCommand::BindShader(RenderPacket.shader);
		RenderCommand::BindMesh(RenderPacket.mesh);

		// rp.Transform 과 Camera VP 행렬 곱
		// 행렬곱의 결과 (MVP Matrix) Constant Buffer 업데이트 필요
		FMatrix MVP = (RenderPacket.model * VP).GetTransposed();
		RenderCommand::UpdateBufferData(Temp.get(), &MVP);
		RenderCommand::BindConstantBuffer(0, Temp.get(), EShaderBindFlagBits::Vertex);

		// Todo: subuv
		if (RenderPacket.bSubUV)
		{
			FSubUVConstants subUVConstans;
			subUVConstans.CurrentFrame = RenderPacket.CurrentFrame;
			subUVConstans.AtlasColSize = RenderPacket.AtlasColumns;
			subUVConstans.AtlasRowSize = RenderPacket.AtlasRows;

			RenderCommand::UpdateBufferData(CB.get(), &subUVConstans);
			RenderCommand::BindConstantBuffer(1, CB.get(), EShaderBindFlagBits::Pixel);
			RenderCommand::BindShaderResource(0, RenderPacket.texture, EShaderBindFlagBits::Pixel);
			RenderCommand::SetBlendState(EBlendState::AlphaBlend);

			RenderCommand::SetDepthStencilState(EDepthStencilState::ReadOnly);
		}
		else
		{
			RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
		}

		RenderCommand::DrawIndexed(RenderPacket.mesh->IndexBuffer->GetIndexCount());
	}
}





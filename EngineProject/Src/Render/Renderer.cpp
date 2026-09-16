#include "EnginePCH.h"
#include "Renderer.h"
#include "Shader.h"
#include "Mesh.h"

#include "RenderCommand.h"

#include "Camera/CameraComponent.h"

#include <algorithm>

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

void FRenderer::RenderAll(TQueue<FRenderPacket>& InQueue, UCameraComponent* CameraComponent)
{
	FMatrix VP = CameraComponent->GetViewProjectionMatrix();
	TArray<FRenderPacket> RenderPackets;

	while (InQueue.IsEmpty() == false)
	{
		RenderPackets.Add(InQueue.Peek());
		InQueue.Dequeue();
	}

	// 불투명 먼저, 반투명은 먼 것부터
	std::stable_sort(
		RenderPackets.begin(),
		RenderPackets.end(),
		[](const FRenderPacket& First, const FRenderPacket& Second) -> bool
		{
			const bool bFirstTranslucent = First.material->BlendState != EBlendState::Opaque;
			const bool bSecondTranslucent = Second.material->BlendState != EBlendState::Opaque;

			if (bFirstTranslucent != bSecondTranslucent)
			{
				return !bFirstTranslucent;
			}

			if (bFirstTranslucent)
			{
				return First.CameraToParticleDistance > Second.CameraToParticleDistance;
			}

			return false;
		});

	for (const FRenderPacket& RenderPacket : RenderPackets)
	{
		RenderCommand::BindMesh(RenderPacket.mesh);
		RenderCommand::BindShader(RenderPacket.material->Shader);
		RenderCommand::SetBlendState(RenderPacket.material->BlendState);

		if (RenderPacket.material->ParamBuffer)
		{
			RenderCommand::BindConstantBuffer(1, RenderPacket.material->ParamBuffer.get(), EShaderBindFlagBits::Pixel);
		}

		for (int i = 0; i < RenderPacket.material->Textures.size(); i++)
		{
			RenderCommand::BindShaderResource(i, RenderPacket.material->Textures[i], EShaderBindFlagBits::Pixel);
		}
		RenderCommand::BindSamplerState(0, ESamplerState::LinearClamp, EShaderBindFlagBits::Pixel);

		// rp.Transform 과 Camera VP 행렬 곱
		// 행렬곱의 결과 (MVP Matrix) Constant Buffer 업데이트 필요
		FMatrix MVP = (RenderPacket.model * VP).GetTransposed();
		RenderCommand::UpdateBufferData(Temp.get(), &MVP);
		RenderCommand::BindConstantBuffer(0, Temp.get(), EShaderBindFlagBits::Vertex);


		// Todo: subuv
		/*if (rp.bSubUV)
		{
			FSubUVConstants subUVConstans;
			subUVConstans.CurrentFrame = rp.CurrentFrame;
			subUVConstans.AtlasColSize = rp.AtlasColumns;
			subUVConstans.AtlasRowSize = rp.AtlasRows;*/

			//RenderCommand::UpdateBufferData(CB.get(), &subUVConstans);
			//RenderCommand::BindConstantBuffer(1, CB.get(), EShaderBindFlagBits::Pixel);
			//RenderCommand::BindShaderResource(0, rp.texture, EShaderBindFlagBits::Pixel);
			//RenderCommand::SetBlendState(EBlendState::AlphaBlend);
		//}

		RenderCommand::DrawIndexed(RenderPacket.mesh->IndexBuffer->GetIndexCount());
	}
}





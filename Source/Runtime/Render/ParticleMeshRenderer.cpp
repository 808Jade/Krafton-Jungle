#include "EnginePCH.h"
#include "ParticleMeshRenderer.h"

#include "Asset/AssetManager.h"
#include "Render/Material.h"
#include "Render/RenderCommand.h"
#include "Render/RenderPacket.h"
#include "Render/RenderResourceManager.h"
#include "Render/Shader.h"
#include "Render/Vertex.h"

#include <cstddef>
#include <cstring>
#include <utility>

bool FParticleMeshRenderer::Init()
{
	ConstantsCB = RenderCommand::CreateConstantBuffer(
	    sizeof(FParticleMeshConstants));

	Shader = FRenderResourceManager::GetShaderProgram(
	    "Resources/Shader/ParticleMeshShader.hlsl");

	if (!ConstantsCB || !ConstantsCB->GetBuffer() ||
	    !Shader ||
	    !Shader->VertexShader ||
	    !Shader->VertexShader->IsValid() ||
	    !Shader->PixelShader ||
	    !Shader->PixelShader->IsValid())
	{
		return false;
	}

	const auto& ByteCode = Shader->VertexShader->GetByteCode();

	if (!ByteCode.IsValid())
		return false;

	D3D11_INPUT_ELEMENT_DESC Elements[7]{};

	Elements[0] = {
		"POSITION", 0,
		DXGI_FORMAT_R32G32B32_FLOAT,
		0, static_cast<UINT>(offsetof(FVertex, Position)),
		D3D11_INPUT_PER_VERTEX_DATA, 0
	};

	Elements[1] = {
		"TEXCOORD", 0,
		DXGI_FORMAT_R32G32_FLOAT,
		0, static_cast<UINT>(offsetof(FVertex, UV)),
		D3D11_INPUT_PER_VERTEX_DATA, 0
	};

	for (UINT Row = 0; Row < 4; ++Row)
	{
		Elements[2 + Row] = {
			"INSTANCE_TRANSFORM", Row,
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			1,
			static_cast<UINT>(
			    offsetof(FMeshParticleInstanceVertex, Transform) + sizeof(float) * 4 * Row),
			D3D11_INPUT_PER_INSTANCE_DATA, 1
		};
	}

	Elements[6] = {
		"INSTANCE_COLOR", 0,
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		1,
		static_cast<UINT>(
		    offsetof(FMeshParticleInstanceVertex, Color)),
		D3D11_INPUT_PER_INSTANCE_DATA, 1
	};

	const HRESULT Result =
	    RenderCommand::GetDevice()->CreateInputLayout(
	        Elements,
	        7,
	        ByteCode.GetData(),
	        ByteCode.GetSize(),
	        InputLayout.GetAddressOf());

	if (FAILED(Result))
		return false;

	return EnsureCapacity(256);
}

bool FParticleMeshRenderer::EnsureCapacity(uint32 RequiredCount)
{
	if (RequiredCount <= InstanceCapacity)
		return true;

	constexpr uint32 MaxInstances = 65535;

	if (RequiredCount > MaxInstances)
		return false;

	uint32 NewCapacity =
	    InstanceCapacity > 0 ? InstanceCapacity : 256;

	while (NewCapacity < RequiredCount)
	{
		NewCapacity = NewCapacity > MaxInstances / 2
		                  ? MaxInstances
		                  : NewCapacity * 2;
	}

	auto NewBuffer = RenderCommand::CreateDynamicVertexBuffer(
	    NewCapacity * sizeof(FMeshParticleInstanceVertex),
	    sizeof(FMeshParticleInstanceVertex));

	if (!NewBuffer || !NewBuffer->GetBuffer())
		return false;

	InstanceVB = std::move(NewBuffer);
	InstanceCapacity = NewCapacity;

	return true;
}

bool FParticleMeshRenderer::Draw(
    const FRenderPacket& Packet,
    const FMatrix& ViewProjection,
    bool bDepthOnly)
{
	if (Packet.MeshInstances.Num() == 0)
		return true;

	auto* VB = Packet.GetVertexBuffer();
	auto* IB = Packet.GetIndexBuffer();
	UMaterial* Material = Packet.Material;

	if (!VB || !VB->GetBuffer() ||
	    !IB || !IB->GetBuffer() ||
	    !Material || Packet.IndexCount == 0)
	{
		return false;
	}

	const uint32 Count =
	    static_cast<uint32>(Packet.MeshInstances.Num());

	if (!EnsureCapacity(Count))
		return false;

	void* Dest = RenderCommand::MapWriteDiscard(InstanceVB.get());

	if (!Dest)
		return false;

	std::memcpy(
	    Dest,
	    Packet.MeshInstances.GetData(),
	    Count * sizeof(FMeshParticleInstanceVertex));

	RenderCommand::Unmap(InstanceVB.get());

	FParticleMeshConstants Constants{};
	Constants.ViewProjection = ViewProjection;
	Constants.BaseColor = Material->BaseColor;
	Constants.bOpaque =
	    Material->BlendState == EBlendState::Opaque ? 1u : 0u;

	RenderCommand::UpdateBufferData(
	    ConstantsCB.get(), &Constants);

	RenderCommand::BindConstantBuffer(
	    3,
	    ConstantsCB.get(),
	    EShaderBindFlagBits::Vertex |
	        EShaderBindFlagBits::Pixel);

	ID3D11DeviceContext* Context = RenderCommand::GetContext();

	Context->VSSetShader(
	    Shader->VertexShader->GetShader(), nullptr, 0);

	Context->PSSetShader(
	    bDepthOnly ? nullptr : Shader->PixelShader->GetShader(),
	    nullptr, 0);

	Context->IASetInputLayout(InputLayout.Get());

	RenderCommand::SetPrimitiveTopology(
	    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D11Buffer* Buffers[2] = {
		VB->GetBuffer(),
		InstanceVB->GetBuffer()
	};

	const UINT Strides[2] = {
		VB->GetStride(),
		static_cast<UINT>(sizeof(FMeshParticleInstanceVertex))
	};

	const UINT Offsets[2] = { 0, 0 };

	Context->IASetVertexBuffers(
	    0, 2, Buffers, Strides, Offsets);

	RenderCommand::BindIndexBuffer(IB);

	const bool bOpaque =
	    Material->BlendState == EBlendState::Opaque;

	RenderCommand::SetBlendState(
	    bDepthOnly ? EBlendState::Opaque : Material->BlendState);

	RenderCommand::SetDepthStencilState(
	    bDepthOnly || bOpaque
	        ? EDepthStencilState::Default
	        : EDepthStencilState::ReadOnly);

	// 호출자가 설정한 SolidBack 또는 WireFrame을 유지하고,
	// 메시렌더러에서 임의로 변경하지 않는다.

	if (!bDepthOnly)
	{
		UTexture2D* Texture =
		    Material->Textures.Num() > 0
		        ? Material->Textures[0]
		        : nullptr;

		if (!Texture)
		{
			Texture = UAssetManager::GetAssetByPath<UTexture2D>(
			    "WhiteTexture");
		}

		if (!Texture)
			return false;

		RenderCommand::BindShaderResource(
		    0, Texture, EShaderBindFlagBits::Pixel);

		RenderCommand::BindSamplerState(
		    0, Material->SamplerState, EShaderBindFlagBits::Pixel);
	}

	RenderCommand::DrawIndexedInstanced(
	    Packet.IndexCount,
	    Count,
	    Packet.StartIndex,
	    0,
	    0);

	ID3D11Buffer* NullBuffer = nullptr;
	const UINT Zero = 0;

	Context->IASetVertexBuffers(
	    1, 1, &NullBuffer, &Zero, &Zero);

	return true;
}

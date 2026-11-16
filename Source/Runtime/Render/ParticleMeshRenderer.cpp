#include "EnginePCH.h"
#include "ParticleMeshRenderer.h"

#include "Core/EngineTimer.h"
#include "Asset/AssetManager.h"
#include "Render/Material.h"
#include "Render/RenderCommand.h"
#include "Render/RenderPacket.h"
#include "Render/RenderResourceManager.h"
#include "Render/Shader.h"
#include "Render/Vertex.h"
#include "Engine/PrimitiveSceneProxy.h"

#include <cstddef>
#include <cstring>
#include <utility>

bool FParticleMeshRenderer::Init()
{
	MaterialCB = RenderCommand::CreateConstantBuffer(
	    sizeof(FStaticMeshMaterialParams));

	BatchCB = RenderCommand::CreateConstantBuffer(
	    sizeof(FParticleMeshBatchConstants));

	if (!MaterialCB || !MaterialCB->GetBuffer() ||
	    !BatchCB || !BatchCB->GetBuffer())
	{
		return false;
	}

	const D3D11_INPUT_ELEMENT_DESC Elements[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,
		  0, static_cast<UINT>(offsetof(FVertex, Position)),
		  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,
		  0, static_cast<UINT>(offsetof(FVertex, Normal)),
		  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,
		  0, static_cast<UINT>(offsetof(FVertex, Tangent)),
		  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,
		  0, static_cast<UINT>(offsetof(FVertex, UV)),
		  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,
		  0, static_cast<UINT>(offsetof(FVertex, Color)),
		  D3D11_INPUT_PER_VERTEX_DATA, 0 },

		{ "INSTANCE_TRANSFORM", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,
		  1, static_cast<UINT>(offsetof(FMeshParticleInstanceVertex, Transform) + 0),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_TRANSFORM", 1, DXGI_FORMAT_R32G32B32A32_FLOAT,
		  1, static_cast<UINT>(offsetof(FMeshParticleInstanceVertex, Transform) + 16),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_TRANSFORM", 2, DXGI_FORMAT_R32G32B32A32_FLOAT,
		  1, static_cast<UINT>(offsetof(FMeshParticleInstanceVertex, Transform) + 32),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_TRANSFORM", 3, DXGI_FORMAT_R32G32B32A32_FLOAT,
		  1, static_cast<UINT>(offsetof(FMeshParticleInstanceVertex, Transform) + 48),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },

		{ "INSTANCE_NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,
		  1, static_cast<UINT>(offsetof(FMeshParticleInstanceVertex, NormalRow0)),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_NORMAL", 1, DXGI_FORMAT_R32G32B32A32_FLOAT,
		  1, static_cast<UINT>(offsetof(FMeshParticleInstanceVertex, NormalRow1)),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_NORMAL", 2, DXGI_FORMAT_R32G32B32A32_FLOAT,
		  1, static_cast<UINT>(offsetof(FMeshParticleInstanceVertex, NormalRow2)),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,
		  1, static_cast<UINT>(offsetof(FMeshParticleInstanceVertex, Color)),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 }
	};

	const EViewMode Modes[] = {
		EViewMode::Unlit,
		EViewMode::Lit_Gouraud,
		EViewMode::Lit_Lambert,
		EViewMode::Lit_BlinnPhong,
		EViewMode::WorldNormal
	};

	for (EViewMode Mode : Modes)
	{
		FShaderProgram* Program =
		    FRenderResourceManager::GetUberLitMeshParticleProgram(Mode);

		if (!Program ||
		    !Program->VertexShader ||
		    !Program->VertexShader->IsValid() ||
		    !Program->PixelShader ||
		    !Program->PixelShader->IsValid())
		{
			return false;
		}

		const auto& ByteCode =
		    Program->VertexShader->GetByteCode();

		if (!ByteCode.IsValid())
			return false;

		const int32 Index = static_cast<int32>(Mode);

		const HRESULT Result =
		    RenderCommand::GetDevice()->CreateInputLayout(
		        Elements,
		        static_cast<UINT>(
		            sizeof(Elements) / sizeof(Elements[0])),
		        ByteCode.GetData(),
		        ByteCode.GetSize(),
		        InputLayouts[Index].GetAddressOf());

		if (FAILED(Result))
			return false;
	}

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
    EViewMode ViewMode,
    bool bDepthOnly)
{
	if (Packet.MeshInstances.Num() == 0)
		return true;

	FVertexBuffer* VB = Packet.GetVertexBuffer();
	FIndexBuffer* IB = Packet.GetIndexBuffer();
	UMaterial* Material = Packet.Material;

	if (!VB || !VB->GetBuffer() ||
	    !IB || !IB->GetBuffer() ||
	    !Material || Packet.IndexCount == 0)
	{
		return false;
	}

	const int32 ModeIndex = static_cast<int32>(ViewMode);

	if (ModeIndex < 0 ||
	    ModeIndex >= static_cast<int32>(EViewMode::Count))
	{
		return false;
	}

	FShaderProgram* Program =
	    FRenderResourceManager::GetUberLitMeshParticleProgram(ViewMode);

	if (!Program || !InputLayouts[ModeIndex].Get())
		return false;

	UTexture2D* DiffuseTexture = nullptr;
	UTexture2D* NormalTexture = nullptr;

	if (!bDepthOnly)
	{
		DiffuseTexture = Material->Textures.Num() > 0
		                     ? Material->Textures[0]
		                     : nullptr;

		NormalTexture = Material->Textures.Num() > 1
		                    ? Material->Textures[1]
		                    : nullptr;

		if (!DiffuseTexture)
		{
			DiffuseTexture =
			    UAssetManager::GetAssetByPath<UTexture2D>("WhiteTexture");
		}

		if (!NormalTexture)
		{
			NormalTexture =
			    UAssetManager::GetAssetByPath<UTexture2D>("FlatNormalTexture");
		}

		if (!DiffuseTexture || !NormalTexture)
			return false;
	}

	const uint32 InstanceCount =
	    static_cast<uint32>(Packet.MeshInstances.Num());

	if (!EnsureCapacity(InstanceCount))
		return false;

	void* Dest = RenderCommand::MapWriteDiscard(InstanceVB.get());

	if (!Dest)
		return false;

	std::memcpy(
	    Dest,
	    Packet.MeshInstances.GetData(),
	    InstanceCount * sizeof(FMeshParticleInstanceVertex));

	RenderCommand::Unmap(InstanceVB.get());

	FParticleMeshBatchConstants Batch{};
	Batch.ReceiveShadow =
	    Packet.Proxy && Packet.Proxy->IsShadowReceiver()
	        ? 1u
	        : 0u;
	
	Batch.ReceiveDecal =
		Packet.Proxy && Packet.Proxy->IsDecalReceiver()
	        ? 1u
	        : 0u;

	RenderCommand::UpdateBufferData(BatchCB.get(), &Batch);

	RenderCommand::BindConstantBuffer(
	    3,
	    BatchCB.get(),
	    EShaderBindFlagBits::Vertex);

	if (!bDepthOnly)
	{
		FStaticMeshMaterialParams Params{};

		Params.BaseColor = Material->BaseColor;
		Params.UVOffset =
		    Material->UVScrollSpeed * EngineTimer::GetTotalTime();

		Params.bOpaque =
		    Material->BlendState == EBlendState::Opaque ? 1.0f : 0.0f;

		Params.Shininess = Material->Shininess;
		Params.WorldUVTileSize = Material->WorldUVTileSize;

		RenderCommand::UpdateBufferData(
		    MaterialCB.get(), &Params);

		// Gouraud 변형 등 VS에서도 참조할 수 있도록 양쪽에 바인딩.
		RenderCommand::BindConstantBuffer(
		    1,
		    MaterialCB.get(),
		    EShaderBindFlagBits::Vertex |
		        EShaderBindFlagBits::Pixel);

		RenderCommand::BindShaderResource(
		    0, DiffuseTexture, EShaderBindFlagBits::Pixel);

		RenderCommand::BindShaderResource(
		    1, NormalTexture, EShaderBindFlagBits::Pixel);

		RenderCommand::BindSamplerState(
		    0, Material->SamplerState, EShaderBindFlagBits::Pixel);
	}

	ID3D11DeviceContext* Context = RenderCommand::GetContext();

	Context->VSSetShader(
	    Program->VertexShader->GetShader(), nullptr, 0);

	Context->PSSetShader(
	    bDepthOnly ? nullptr : Program->PixelShader->GetShader(),
	    nullptr, 0);

	Context->IASetInputLayout(InputLayouts[ModeIndex].Get());

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

	const bool bTranslucent =
	    Material->BlendState != EBlendState::Opaque;

	RenderCommand::SetBlendState(
	    bDepthOnly ? EBlendState::Opaque : Material->BlendState);

	const EDepthStencilState DepthState =
	    bDepthOnly
	        ? EDepthStencilState::Default
	        : (
	              bTranslucent &&
	                      Material->DepthStencilState == EDepthStencilState::Default
	                  ? EDepthStencilState::ReadOnly
	                  : Material->DepthStencilState);

	RenderCommand::SetDepthStencilState(DepthState);

	RenderCommand::DrawIndexedInstanced(
	    Packet.IndexCount,
	    InstanceCount,
	    Packet.StartIndex,
	    0,
	    0);

	ID3D11Buffer* NullBuffer = nullptr;
	const UINT Zero = 0;

	Context->IASetVertexBuffers(
	    1, 1, &NullBuffer, &Zero, &Zero);

	return true;
}

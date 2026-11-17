#include "EnginePCH.h"
#include "Render/ParticleStripRenderer.h"

#include "Render/ParticleSpriteRenderer.h"
#include "Render/Material.h"
#include "Render/RenderCommand.h"
#include "Render/RenderResourceManager.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <utility>

bool FParticleStripRenderer::Init()
{
	ConstantsBuffer = RenderCommand::CreateConstantBuffer(
	    sizeof(FParticleStripConstants));

	Shader = FRenderResourceManager::GetShaderProgram(
	    "Resources/Shader/ParticleStripShader.hlsl");

	if (!ConstantsBuffer || !ConstantsBuffer->GetBuffer() ||
	    !Shader ||
	    !Shader->VertexShader ||
	    !Shader->VertexShader->IsValid() ||
	    !Shader->PixelShader ||
	    !Shader->PixelShader->IsValid())
	{
		return false;
	}

	const FShaderByteCode& VSCode =
	    Shader->VertexShader->GetByteCode();

	if (!VSCode.IsValid())
	{
		return false;
	}

	const D3D11_INPUT_ELEMENT_DESC Elements[] = {
		{ "POSITION", 0,
		  DXGI_FORMAT_R32G32B32_FLOAT,
		  0,
		  static_cast<UINT>(
		      offsetof(FParticleStripVertex, Position)),
		  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0,
		  DXGI_FORMAT_R32G32_FLOAT,
		  0,
		  static_cast<UINT>(
		      offsetof(FParticleStripVertex, UV)),
		  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0,
		  DXGI_FORMAT_R32G32B32A32_FLOAT,
		  0,
		  static_cast<UINT>(
		      offsetof(FParticleStripVertex, Color)),
		  D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	const HRESULT Result =
	    RenderCommand::GetDevice()->CreateInputLayout(
	        Elements,
	        static_cast<UINT>(
	            sizeof(Elements) / sizeof(Elements[0])),
	        VSCode.GetData(),
	        VSCode.GetSize(),
	        InputLayout.GetAddressOf());

	if (FAILED(Result))
	{
		return false;
	}

	return EnsureCapacity(64);
}

bool FParticleStripRenderer::EnsureCapacity(
    uint32 RequiredPointCount)
{
	if (RequiredPointCount <= PointCapacity)
	{
		return true;
	}

	// 실제 파티클 최대 개수 + 렌더링용 Source 점 하나.
	// Index Buffer 자체는 uint32 인덱스를 사용한다.
	constexpr uint32 MaxPoints = 65536;

	if (RequiredPointCount > MaxPoints)
	{
		return false;
	}

	uint32 NewCapacity =
	    PointCapacity > 0 ? PointCapacity : 64;

	while (NewCapacity < RequiredPointCount)
	{
		NewCapacity = std::min(NewCapacity * 2, MaxPoints);
	}

	auto NewVB = RenderCommand::CreateDynamicVertexBuffer(
	    NewCapacity * 2 *
	        static_cast<uint32>(sizeof(FParticleStripVertex)),
	    sizeof(FParticleStripVertex));

	if (!NewVB || !NewVB->GetBuffer())
	{
		return false;
	}

	TArray<uint32> Indices;
	Indices.Reserve((NewCapacity - 1) * 6);

	for (uint32 PointIndex = 0;
	     PointIndex + 1 < NewCapacity;
	     ++PointIndex)
	{
		const uint32 Left0 = PointIndex * 2;
		const uint32 Right0 = Left0 + 1;
		const uint32 Left1 = Left0 + 2;
		const uint32 Right1 = Left0 + 3;

		Indices.Add(Left0);
		Indices.Add(Right0);
		Indices.Add(Left1);

		Indices.Add(Right0);
		Indices.Add(Right1);
		Indices.Add(Left1);
	}

	auto NewIB = RenderCommand::CreateStaticIndexBuffer(
	    Indices.GetData(),
	    static_cast<uint32>(Indices.Num()));

	if (!NewIB || !NewIB->GetBuffer())
	{
		return false;
	}

	VertexBuffer = std::move(NewVB);
	IndexBuffer = std::move(NewIB);
	PointCapacity = NewCapacity;

	return true;
}

void FParticleStripRenderer::BuildVertices(
    const TArray<FParticleStripPoint>& Points,
    const FParticleSpriteView& View)
{
	constexpr float EpsilonSq = 1.0e-8f;

	CleanPoints.Reset();
	Vertices.Reset();

	// 연속으로 겹친 점은 제거한다.
	// 길이 0인 선분의 정규화를 피하기 위한 처리.
	for (const FParticleStripPoint& Point : Points)
	{
		if (CleanPoints.Num() > 0)
		{
			const FVector Delta =
			    Point.Position -
			    CleanPoints[CleanPoints.Num() - 1].Position;

			if (Delta.SizeSquared() <= EpsilonSq)
			{
				continue;
			}
		}

		CleanPoints.Add(Point);
	}

	if (CleanPoints.Num() < 2)
	{
		return;
	}

	Vertices.SetNum(CleanPoints.Num() * 2);

	FVector PreviousSide = FVector::ZeroVector;

	for (int32 Index = 0; Index < CleanPoints.Num(); ++Index)
	{
		const FParticleStripPoint& Point = CleanPoints[Index];

		FVector Tangent;

		if (Index == 0)
		{
			Tangent =
			    CleanPoints[1].Position - Point.Position;
		}
		else if (Index == CleanPoints.Num() - 1)
		{
			Tangent =
			    Point.Position - CleanPoints[Index - 1].Position;
		}
		else
		{
			Tangent =
			    CleanPoints[Index + 1].Position -
			    CleanPoints[Index - 1].Position;

			// 경로가 되돌아와 앞뒤 점이 겹치는 경우.
			if (Tangent.SizeSquared() <= EpsilonSq)
			{
				Tangent =
				    CleanPoints[Index + 1].Position -
				    Point.Position;
			}
		}

		Tangent = Tangent.Normalized();

		FVector ToCamera = View.bOrthographic
		                       ? View.CameraForward * -1.0f
		                       : View.CameraPosition - Point.Position;

		if (ToCamera.SizeSquared() <= EpsilonSq)
		{
			ToCamera = View.CameraForward * -1.0f;
		}

		ToCamera = ToCamera.Normalized();

		FVector Side = FVector::Cross(Tangent, ToCamera);

		// 선분 방향과 시선이 평행할 때 사용할 보조 축.
		if (Side.SizeSquared() <= EpsilonSq)
		{
			const FVector FallbackAxis =
			    std::abs(Tangent.Z) < 0.9f
			        ? FVector(0.0f, 0.0f, 1.0f)
			        : FVector(0.0f, 1.0f, 0.0f);

			Side = FVector::Cross(Tangent, FallbackAxis);
		}

		Side = Side.Normalized();

		// 인접 점 사이에서 좌우 방향이 갑자기 뒤집히는 것을 완화.
		if (Index > 0 &&
		    FVector::Dot(Side, PreviousSide) < 0.0f)
		{
			Side *= -1.0f;
		}

		PreviousSide = Side;

		const FVector Offset =
		    Side * (std::abs(Point.Width) * 0.5f);

		const FVector4 Color(
		    Point.Color.R,
		    Point.Color.G,
		    Point.Color.B,
		    Point.Color.A);

		FParticleStripVertex Left{};
		Left.Position = Point.Position - Offset;
		Left.UV = FVector2(Point.U, 0.0f);
		Left.Color = Color;

		FParticleStripVertex Right{};
		Right.Position = Point.Position + Offset;
		Right.UV = FVector2(Point.U, 1.0f);
		Right.Color = Color;

		Vertices[Index * 2] = Left;
		Vertices[Index * 2 + 1] = Right;
	}
}

bool FParticleStripRenderer::Draw(
    const TArray<FParticleStripPoint>& Points,
    UMaterial* Material,
    const FParticleSpriteView& View)
{
	if (!Material || !Shader || !ConstantsBuffer)
	{
		return false;
	}

	if (Material->BlendState != EBlendState::AlphaBlend &&
	    Material->BlendState != EBlendState::Additive)
	{
		return false;
	}

	if (Points.Num() > 65536)
	{
		return false;
	}

	BuildVertices(Points, View);

	if (CleanPoints.Num() < 2)
	{
		// 모든 점이 겹친 경우 등: 그릴 면이 없다.
		return true;
	}

	const uint32 PointCount =
	    static_cast<uint32>(CleanPoints.Num());

	if (!EnsureCapacity(PointCount))
	{
		return false;
	}

	void* Dest =
	    RenderCommand::MapWriteDiscard(VertexBuffer.get());

	if (!Dest)
	{
		return false;
	}

	std::memcpy(
	    Dest,
	    Vertices.GetData(),
	    static_cast<size_t>(Vertices.Num()) *
	        sizeof(FParticleStripVertex));

	RenderCommand::Unmap(VertexBuffer.get());

	const bool bUseTexture =
	    Material->Textures.Num() > 0 &&
	    Material->Textures[0] != nullptr;

	FParticleStripConstants Constants{};
	Constants.ViewProjection = View.ViewProjection;
	Constants.MaterialColor = Material->BaseColor;
	Constants.UseTexture = bUseTexture ? 1u : 0u;

	RenderCommand::UpdateBufferData(
	    ConstantsBuffer.get(),
	    &Constants);

	RenderCommand::BindConstantBuffer(
	    3,
	    ConstantsBuffer.get(),
	    EShaderBindFlagBits::Vertex |
	        EShaderBindFlagBits::Pixel);

	ID3D11DeviceContext* Context =
	    RenderCommand::GetContext();

	Context->VSSetShader(
	    Shader->VertexShader->GetShader(), nullptr, 0);

	Context->PSSetShader(
	    Shader->PixelShader->GetShader(), nullptr, 0);

	Context->GSSetShader(nullptr, nullptr, 0);

	Context->IASetInputLayout(InputLayout.Get());

	RenderCommand::SetPrimitiveTopology(
	    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D11Buffer* VB = VertexBuffer->GetBuffer();
	const UINT Stride = sizeof(FParticleStripVertex);
	const UINT Offset = 0;

	Context->IASetVertexBuffers(
	    0, 1, &VB, &Stride, &Offset);

	RenderCommand::BindIndexBuffer(IndexBuffer.get());

	RenderCommand::SetRasterizerState(
	    ERasterizerState::SolidNone);

	RenderCommand::SetBlendState(Material->BlendState);

	RenderCommand::SetDepthStencilState(
	    EDepthStencilState::ReadOnly);

	if (bUseTexture)
	{
		RenderCommand::BindSamplerState(
		    0,
		    Material->SamplerState,
		    EShaderBindFlagBits::Pixel);

		RenderCommand::BindShaderResource(
		    0,
		    Material->Textures[0],
		    EShaderBindFlagBits::Pixel);
	}
	else
	{
		ID3D11ShaderResourceView* NullSRV = nullptr;
		Context->PSSetShaderResources(0, 1, &NullSRV);
	}

	RenderCommand::DrawIndexed((PointCount - 1) * 6);

	return true;
}

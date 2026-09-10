#include "EnginePCH.h"
#include "GridRenderer.h"

#include "Render/Vertex.h"
#include "Engine/ResourceManager.h"

FGridRenderer::~FGridRenderer()
{
}

bool FGridRenderer::Init(FRenderer* InRenderer)
{
	Renderer = InRenderer;

	GridShader = Renderer->CreateShader(L"Shader/GridShader.hlsl", nullptr, 0);
	AxisShader = FResourceManager::GetInstance().GetShader("Shader/DefaultShader.hlsl");
	TArray<FVertex> Vertices =
	{
		// X Axis
		{ { 0.0f, 0.0f, 0.0f },{ 0.85f, 0.20f, 0.20f, 1.0f } },
		{ { 100.0f, 0.0f, 0.0f },{ 0.85f, 0.20f, 0.20f, 1.0f } },

		// Y Axis
		{ { 0.0f, 0.0f, 0.0f },{ 0.25f, 0.80f, 0.35f, 1.0f } },
		{ { 0.0f, 100.0f, 0.0f },{ 0.25f, 0.80f, 0.35f, 1.0f } },

		// Z Axis
		{ { 0.0f, 0.0f, 0.0f },{ 0.25f, 0.45f, 0.90f, 1.0f } },
		{ { 0.0f, 0.0f, 100.0f },{ 0.25f, 0.45f, 0.90f, 1.0f } },
	};

	TArray<uint32> Indices =
	{
		0,1,2,3,4,5
	};

	VB = Renderer->CreateVertexBuffer(Vertices.data(), sizeof(FVertex) * Vertices.size(), sizeof(FVertex));
	IB = Renderer->CreateIndexBuffer(Indices.data(), sizeof(uint32) * Indices.size());
	CB = Renderer->CreateConstantBuffer(sizeof(GridData));

	D3D11_RASTERIZER_DESC RasterizerDesc = {};
	RasterizerDesc.FillMode = D3D11_FILL_SOLID;
	RasterizerDesc.CullMode = D3D11_CULL_NONE;
	RasterizerDesc.FrontCounterClockwise = FALSE;
	Renderer->GetDevice()->CreateRasterizerState(&RasterizerDesc, &RasterizerState);


	return false;
}

void FGridRenderer::OnRender(const FMatrix& ViewProj, const FVector& CameraPos)
{
	GridData Data;
	Data.invViewProj = ViewProj.GetTransposed();
	Data.CameraPos = CameraPos;

	// 1. 무한 그리드 (바닥 격자) - 버텍스 버퍼 없음
	Renderer->BindShader(GridShader.get());
	Renderer->BindVertexBuffer(nullptr);
	Renderer->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Renderer->UpdateConstantBufferData(CB.get(), &Data, sizeof(GridData));
	Renderer->BindConstantBuffer(0, CB.get(), EShaderBindFlagBits::Vertex); 
	Renderer->BindConstantBuffer(0, CB.get(), EShaderBindFlagBits::Pixel);
	Renderer->GetDeviceContext()->RSSetState(RasterizerState.Get());
	Renderer->Draw(4);
	Renderer->GetDeviceContext()->RSSetState(nullptr);

	Renderer->BindShader(AxisShader);
	Renderer->BindVertexBuffer(VB.get());
	Renderer->BindIndexBuffer(IB.get());
	Renderer->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	Renderer->UpdateConstantBuffer(Identity * ViewProj);
	Renderer->DrawIndexed(6);
}

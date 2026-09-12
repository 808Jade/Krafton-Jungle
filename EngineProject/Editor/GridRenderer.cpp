#include "EnginePCH.h"
#include "GridRenderer.h"

#include "Render/Vertex.h"
#include "Asset/AssetManager.h"

FGridRenderer::~FGridRenderer()
{
}

bool FGridRenderer::Init(FRenderer* InRenderer)
{
	Renderer = InRenderer;

	GridShader = Renderer->CreateShader(L"Shader/GridShader.hlsl", nullptr, 0);
	AxisShader = UAssetManager::Get().GetShader("Shader/DefaultShader.hlsl");
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

	VB = Renderer->CreateVertexBuffer(Vertices.GetData(), sizeof(FVertex) * Vertices.Num(), sizeof(FVertex));
	IB = Renderer->CreateIndexBuffer(Indices.GetData(), sizeof(uint32) * Indices.Num());
	CB = Renderer->CreateConstantBuffer(sizeof(GridData));

	D3D11_RASTERIZER_DESC RasterizerDesc = {};
	RasterizerDesc.FillMode = D3D11_FILL_SOLID;
	RasterizerDesc.CullMode = D3D11_CULL_NONE;
	RasterizerDesc.FrontCounterClockwise = FALSE;
	Renderer->GetDevice()->CreateRasterizerState(&RasterizerDesc, &RasterizerState);

	return false;
}

void FGridRenderer::OnRender(const FMatrix& ViewProj, const FVector& CameraPos, const FEditorSettings& InEditorSettings)
{
	GridData Data;
	Data.invViewProj = ViewProj.GetTransposed();
	Data.CameraPos = CameraPos;
	Data.CellSize = InEditorSettings.GridSpacing;
	Data.SubCellSize = InEditorSettings.GridSpacing * 0.1f;

	Renderer->BindShader(GridShader.get());
	Renderer->BindVertexBuffer(nullptr);
	Renderer->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Renderer->GetDeviceContext()->RSSetState(RasterizerState.Get());

	Data.GridPlaneType = 0;
	Renderer->UpdateConstantBufferData(CB.get(), &Data, sizeof(GridData));
	Renderer->BindConstantBuffer(0, CB.get(), EShaderBindFlagBits::Vertex);
	Renderer->BindConstantBuffer(0, CB.get(), EShaderBindFlagBits::Pixel);
	Renderer->SetBlendStateEnabled(true);
	Renderer->Draw(4);

	Data.GridPlaneType = 1;
	Renderer->UpdateConstantBufferData(CB.get(), &Data, sizeof(GridData));
	Renderer->Draw(4);

	Renderer->SetBlendStateEnabled(false);
	Renderer->GetDeviceContext()->RSSetState(nullptr);
}
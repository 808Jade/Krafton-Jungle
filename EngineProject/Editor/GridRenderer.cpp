#include "EnginePCH.h"
#include "GridRenderer.h"

#include "Render/Vertex.h"
#include "Asset/AssetManager.h"

FGridRenderer::~FGridRenderer()
{
	delete[] BatchGridVertices;
}

bool FGridRenderer::Init(FRenderer* InRenderer)
{
	Renderer = InRenderer;

	// Batch Grid
	BatchGridShader = Renderer->CreateShader(L"Shader/GridShaderBatch.hlsl", FVertex::GetLayout());
	MaxVertices = 65536;
	BatchGridVertices = new FVertex[MaxVertices];
	BatchGridVertexBuffer = Renderer->CreateDynamicVertexBuffer(sizeof(FVertex) * MaxVertices, sizeof(FVertex));
	BatchGridConstantBuffer = Renderer->CreateConstantBuffer(sizeof(FBatchGridData));


	// PS Grid
	PSGridShader = Renderer->CreateShader(L"Shader/GridShader.hlsl", nullptr, 0);
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

	PSGridVertexBuffer = Renderer->CreateVertexBuffer(Vertices.GetData(), sizeof(FVertex) * Vertices.Num(), sizeof(FVertex));
	PSGridIndexBuffer = Renderer->CreateIndexBuffer(Indices.GetData(), sizeof(uint32) * Indices.Num());
	PSGridConstantBuffer = Renderer->CreateConstantBuffer(sizeof(FPSGridData));

	D3D11_RASTERIZER_DESC RasterizerDesc = {};
	RasterizerDesc.FillMode = D3D11_FILL_SOLID;
	RasterizerDesc.CullMode = D3D11_CULL_NONE;
	RasterizerDesc.FrontCounterClockwise = FALSE;
	Renderer->GetDevice()->CreateRasterizerState(&RasterizerDesc, &RasterizerState);

	return true;
}

void FGridRenderer::OnRenderBatchGrid(const FMatrix& ViewProj, const FVector& CameraPos)
{
	FBatchGridData Data;
	Data.invViewProj = ViewProj.GetTransposed();
	Data.CameraPos = CameraPos;
	Data.Padding = BatchGridSpace;

	FVertex* VertexCursor = BatchGridVertices;
	uint32 VertexCount = 0;

	for (int i = -200; i <= 200; i++)
	{
		*VertexCursor++ = { FVector(i * BatchGridSpace, -200.0f * BatchGridSpace, 0.0f), FVector4(1.0f, 1.0f, 1.0f, 1.0f) };
		*VertexCursor++ = { FVector(i * BatchGridSpace, 200.0f * BatchGridSpace, 0.0f), FVector4(1.0, 1.0f, 1.0f, 1.0f) };

		*VertexCursor++ = { FVector(-200.0f * BatchGridSpace, i * BatchGridSpace, 0.0f), FVector4(1.0f, 1.0f, 1.0f, 1.0f) };
		*VertexCursor++ = { FVector(200.0f * BatchGridSpace, i * BatchGridSpace, 0.0f), FVector4(1.0f, 1.0f, 1.0f, 1.0f) };

		VertexCount += 4;
	}

	Renderer->UpdateVertexBufferData(BatchGridVertexBuffer.get(), BatchGridVertices, sizeof(FVertex) * VertexCount);
	Renderer->UpdateConstantBufferData(BatchGridConstantBuffer.get(), &Data, sizeof(FBatchGridData));

	Renderer->BindShader(BatchGridShader.get());
	Renderer->BindVertexBuffer(BatchGridVertexBuffer.get());
	Renderer->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	Renderer->BindConstantBuffer(0, BatchGridConstantBuffer.get(), EShaderBindFlagBits::Vertex);
	Renderer->Draw(VertexCount);
}

void FGridRenderer::OnRenderPSGrid(const FMatrix& ViewProj, const FVector& CameraPos, const FEditorSettings& InEditorSettings)
{
	FPSGridData PSGridData;
	PSGridData.invViewProj = ViewProj.GetTransposed();
	PSGridData.CameraPos = CameraPos;
	PSGridData.CellSize = InEditorSettings.GridSpacing;
	PSGridData.SubCellSize = InEditorSettings.GridSpacing * 0.1f;

	Renderer->BindShader(PSGridShader.get());
	Renderer->BindVertexBuffer(nullptr);
	Renderer->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Renderer->GetDeviceContext()->RSSetState(RasterizerState.Get());

	PSGridData.GridPlaneType = 0;
	Renderer->UpdateConstantBufferData(PSGridConstantBuffer.get(), &PSGridData, sizeof(FPSGridData));
	Renderer->BindConstantBuffer(0, PSGridConstantBuffer.get(), EShaderBindFlagBits::Vertex);
	Renderer->BindConstantBuffer(0, PSGridConstantBuffer.get(), EShaderBindFlagBits::Pixel);
	Renderer->SetBlendStateEnabled(true);
	Renderer->Draw(4);

	PSGridData.GridPlaneType = 1;
	Renderer->UpdateConstantBufferData(PSGridConstantBuffer.get(), &PSGridData, sizeof(FPSGridData));
	Renderer->Draw(4);

	Renderer->SetBlendStateEnabled(false);
	Renderer->GetDeviceContext()->RSSetState(nullptr);
}
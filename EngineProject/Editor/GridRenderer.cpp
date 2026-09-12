#include "EnginePCH.h"
#include "GridRenderer.h"

#include "Render/Vertex.h"
#include "Asset/AssetManager.h"

FGridRenderer::~FGridRenderer()
{
	delete[] VertexBufferBase;
}

bool FGridRenderer::Init(FRenderer* InRenderer)
{
	Renderer = InRenderer;

	BatchGridShader = Renderer->CreateShader(L"Shader/GridShaderBatch.hlsl", FVertex::GetLayout());

	MaxVertices = 65536;
	VertexBufferBase = new FVertex[MaxVertices];

	VertexBuffer = Renderer->CreateDynamicVertexBuffer(sizeof(FVertex) * MaxVertices, sizeof(FVertex));

	CB = Renderer->CreateConstantBuffer(sizeof(FGridData));

	////////////////////////////////////////////// 복원용 주석
	//GridShader = Renderer->CreateShader(L"Shader/GridShader.hlsl", nullptr, 0);
	//AxisShader = UAssetManager::Get().GetShader("Shader/DefaultShader.hlsl");
	//TArray<FVertex> Vertices =
	//{
	//	// X Axis
	//	{ { 0.0f, 0.0f, 0.0f },{ 0.85f, 0.20f, 0.20f, 1.0f } },
	//	{ { 100.0f, 0.0f, 0.0f },{ 0.85f, 0.20f, 0.20f, 1.0f } },

	//	// Y Axis
	//	{ { 0.0f, 0.0f, 0.0f },{ 0.25f, 0.80f, 0.35f, 1.0f } },
	//	{ { 0.0f, 100.0f, 0.0f },{ 0.25f, 0.80f, 0.35f, 1.0f } },

	//	// Z Axis
	//	{ { 0.0f, 0.0f, 0.0f },{ 0.25f, 0.45f, 0.90f, 1.0f } },
	//	{ { 0.0f, 0.0f, 100.0f },{ 0.25f, 0.45f, 0.90f, 1.0f } },
	//};

	//TArray<uint32> Indices =
	//{
	//	0,1,2,3,4,5
	//};

	//VB = Renderer->CreateVertexBuffer(Vertices.GetData(), sizeof(FVertex) * Vertices.Num(), sizeof(FVertex));
	//IB = Renderer->CreateIndexBuffer(Indices.GetData(), sizeof(uint32) * Indices.Num());
	//CB = Renderer->CreateConstantBuffer(sizeof(GridData));

	//D3D11_RASTERIZER_DESC RasterizerDesc = {};
	//RasterizerDesc.FillMode = D3D11_FILL_SOLID;
	//RasterizerDesc.CullMode = D3D11_CULL_NONE;
	//RasterizerDesc.FrontCounterClockwise = FALSE;
	//Renderer->GetDevice()->CreateRasterizerState(&RasterizerDesc, &RasterizerState);
	////////////////////////////////////////////// 복원용 주석

	return true;
}

void FGridRenderer::OnRender(const FMatrix& ViewProj, const FVector& CameraPos, const FEditorSettings& InEditorSettings)
{
	FGridData Data;
	Data.invViewProj = ViewProj.GetTransposed();
	Data.CameraPos = CameraPos;
	Data.padding = GridSpace;

	VertexBufferPtr = VertexBufferBase;
	VertexCount = 0;

	for (int i = -200; i <= 200; i++)
	{
		*VertexBufferPtr++ = { FVector(i * GridSpace, -200.0f * GridSpace, 0.0f), FVector4(1.0f, 1.0f, 1.0f, 1.0f) };
		*VertexBufferPtr++ = { FVector(i * GridSpace, 200.0f * GridSpace, 0.0f), FVector4(1.0, 1.0f, 1.0f, 1.0f) };

		*VertexBufferPtr++ = { FVector(-200.0f * GridSpace, i * GridSpace, 0.0f), FVector4(1.0f, 1.0f, 1.0f, 1.0f) };
		*VertexBufferPtr++ = { FVector(200.0f * GridSpace, i * GridSpace, 0.0f), FVector4(1.0f, 1.0f, 1.0f, 1.0f) };

		VertexCount += 4;
	}

	Renderer->UpdateVertexBufferData(VertexBuffer.get(), VertexBufferBase, sizeof(FVertex) * VertexCount);
	Renderer->UpdateConstantBufferData(CB.get(), &Data, sizeof(FGridData));

	Renderer->BindShader(BatchGridShader.get());
	Renderer->BindVertexBuffer(VertexBuffer.get());
	Renderer->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	Renderer->BindConstantBuffer(0, CB.get(), EShaderBindFlagBits::Vertex);
	Renderer->Draw(VertexCount);


	Data.CellSize = InEditorSettings.GridSpacing;
	Data.SubCellSize = InEditorSettings.GridSpacing * 0.1f;

	// 1. 무한 그리드 (바닥 격자) - 버텍스 버퍼 없음
	//Renderer->BindShader(GridShader.get());
	//Renderer->BindVertexBuffer(nullptr);
	//Renderer->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	//Renderer->UpdateConstantBufferData(CB.get(), &Data, sizeof(FGridData));
	//Renderer->BindConstantBuffer(0, CB.get(), EShaderBindFlagBits::Vertex); 
	//Renderer->BindConstantBuffer(0, CB.get(), EShaderBindFlagBits::Pixel);
	//Renderer->GetDeviceContext()->RSSetState(RasterizerState.Get());
	//Renderer->Draw(4);
	//Renderer->GetDeviceContext()->RSSetState(nullptr);

	//Renderer->BindShader(AxisShader);
	//Renderer->BindVertexBuffer(VB.get());
	//Renderer->BindIndexBuffer(IB.get());
	//Renderer->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	//Renderer->UpdateConstantBuffer(Identity * ViewProj);
	//Renderer->DrawIndexed(6);
	////////////////////////////////////////////// 복원용 주석
}
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
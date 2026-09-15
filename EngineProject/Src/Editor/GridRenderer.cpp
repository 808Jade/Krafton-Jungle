#include "EnginePCH.h"
#include "GridRenderer.h"

#include "Render/Vertex.h"
#include "Asset/AssetManager.h"

#include "Render/RenderCommand.h"

FGridRenderer::~FGridRenderer()
{
	delete[] BatchGridVertices;
}

bool FGridRenderer::Init(FRenderer* InRenderer)
{
	Renderer = InRenderer;

	// Batch Grid
	BatchGridShader = RenderCommand::CreateShader(L"Shader/GridShaderBatch.hlsl", FVertex::GetLayout());

	MaxVertices = 65536;
	BatchGridVertices = new FVertex[MaxVertices];
	BatchGridVertexBuffer = RenderCommand::CreateDynamicVertexBuffer(sizeof(FVertex) * MaxVertices, sizeof(FVertex));
	BatchGridConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FBatchGridData));

	// PS Grid
	PSGridShader = RenderCommand::CreateShader(L"Shader/GridShader.hlsl", nullptr, 0);
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

	PSGridVertexBuffer = RenderCommand::CreateStaticVertexBuffer(Vertices.GetData(), sizeof(FVertex) * Vertices.Num(), sizeof(FVertex));
	PSGridIndexBuffer = RenderCommand::CreateStaticIndexBuffer(Indices.GetData(), Indices.Num());
	PSGridConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FPSGridData));

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

	RenderCommand::UpdateBufferData(BatchGridVertexBuffer.get(), BatchGridVertices, sizeof(FVertex) * VertexCount);
	RenderCommand::UpdateBufferData(BatchGridConstantBuffer.get(), &Data, sizeof(FBatchGridData));

	RenderCommand::BindShader(BatchGridShader.get());
	RenderCommand::BindVertexBuffer(BatchGridVertexBuffer.get());
	RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	RenderCommand::BindConstantBuffer(0, BatchGridConstantBuffer.get(), EShaderBindFlagBits::Vertex);
	RenderCommand::Draw(VertexCount);
}

void FGridRenderer::OnRenderPSGrid(const FMatrix& ViewProj, const FVector& CameraPos, const FEditorSettings& InEditorSettings)
{
	FPSGridData PSGridData;
	PSGridData.invViewProj = ViewProj.GetTransposed();
	PSGridData.CameraPos = CameraPos;
	PSGridData.CellSize = InEditorSettings.GridSpacing;
	PSGridData.SubCellSize = InEditorSettings.GridSpacing * 0.1f;

	RenderCommand::BindShader(PSGridShader.get());
	RenderCommand::BindVertexBuffer(nullptr);
	RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	RenderCommand::SetRasterizerState(ERasterizerState::SolidNone);

	PSGridData.GridPlaneType = 0;
	RenderCommand::UpdateBufferData(PSGridConstantBuffer.get(), &PSGridData, sizeof(FPSGridData));
	RenderCommand::BindConstantBuffer(0, PSGridConstantBuffer.get(), EShaderBindFlagBits::Vertex);
	RenderCommand::BindConstantBuffer(0, PSGridConstantBuffer.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::SetBlendState(EBlendState::AlphaBlend);
	RenderCommand::Draw(4);

	PSGridData.GridPlaneType = 1;
	RenderCommand::UpdateBufferData(PSGridConstantBuffer.get(), &PSGridData, sizeof(FPSGridData));
	RenderCommand::Draw(4);

	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);

}

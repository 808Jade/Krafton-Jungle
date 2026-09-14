#include "DebugRenderer.h"
#include "EnginePCH.h"
#include "Component/PrimitiveComponent.h"

FDebugRenderer::~FDebugRenderer()
{
	delete[] VertexBufferBase;
}

bool FDebugRenderer::Init(FRenderer* InRenderer, UWorld* InWorld)
{
	Renderer = InRenderer;
	World = InWorld;

	LineShader = Renderer->CreateShader(L"Shader/BoundingBoxShader.hlsl", FVertex::GetLayout());

	MaxVertices = 65536;
	VertexBufferBase = new FVertex[MaxVertices];

	VertexBuffer = Renderer->CreateDynamicVertexBuffer(sizeof(FVertex) * MaxVertices, sizeof(FVertex));

	CB = Renderer->CreateConstantBuffer(sizeof(FMatrix));

	return true;
}

void FDebugRenderer::BuildVertexBuffer()
{
	VertexBufferPtr = VertexBufferBase;
	VertexCount = 0;

	for (UPrimitiveComponent* Prim : World->GetPrimitiveComponents())
	{
		const FTransform& Transform = Prim->GetTransform();
		FBox WorldBox = Prim->GetMeshData().GetAABB().GetWorldAABB(Transform.GetWorldMatrix());

		AddBox(WorldBox, FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	}
}

void FDebugRenderer::AddLine(const FVector& A, const FVector& B, const FVector4& Color)
{
	if (VertexCount + 2 > MaxVertices)
		return;   // 넘치면 버림

	*VertexBufferPtr++ = { A, Color };
	*VertexBufferPtr++ = { B, Color };

	VertexCount += 2;
}

void FDebugRenderer::AddBox(FBox BoundingBox, FVector4 Color)
{
	const FVector& Min = BoundingBox.Min;
	const FVector& Max = BoundingBox.Max;

	FVector C[8] = {
		{ Min.X, Min.Y, Min.Z }, { Max.X, Min.Y, Min.Z },
		{ Max.X, Max.Y, Min.Z }, { Min.X, Max.Y, Min.Z },
		{ Min.X, Min.Y, Max.Z }, { Max.X, Min.Y, Max.Z },
		{ Max.X, Max.Y, Max.Z }, { Min.X, Max.Y, Max.Z },
	};

	static const int E[12][2] = {
		{0,1},{1,2},{2,3},{3,0},      // 아랫면
		{4,5},{5,6},{6,7},{7,4},      // 윗면
		{0,4},{1,5},{2,6},{3,7},      // 기둥
	};

	for (const auto& Edge : E)
		AddLine(C[Edge[0]], C[Edge[1]], Color);
}

void FDebugRenderer::Flush()
{
	Renderer->BindShader(LineShader.get());
	Renderer->BindVertexBuffer(VertexBuffer.get());
	Renderer->BindConstantBuffer(0, CB.get(), Vertex);

	Renderer->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	Renderer->Draw(VertexCount, 0);


}

void FDebugRenderer::OnRender(const FMatrix& ViewProjection)
{
	BuildVertexBuffer();

	FMatrix VP = ViewProjection.GetTransposed();
	Renderer->UpdateVertexBufferData(VertexBuffer.get(), VertexBufferBase, sizeof(FVertex) * VertexCount);
	Renderer->UpdateConstantBufferData(CB.get(), &VP, sizeof(FMatrix));

	Flush();
}


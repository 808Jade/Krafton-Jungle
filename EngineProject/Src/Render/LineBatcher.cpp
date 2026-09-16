#include "EnginePCH.h"
#include "LineBatcher.h"
#include "Component/PrimitiveComponent.h"

#include "RenderCommand.h"

FLineBatcher::~FLineBatcher()
{
	delete[] VertexBufferBase;
}

bool FLineBatcher::Init(FRenderer* InRenderer, UWorld* InWorld)
{
	Renderer = InRenderer;
	World = InWorld;

	LineShader = RenderCommand::CreateShader(L"Shader/BoundingBoxShader.hlsl", FVertex::GetLayout());

	MaxVertices = 65536;
	VertexBufferBase = new FVertex[MaxVertices];

	VertexBuffer = RenderCommand::CreateDynamicVertexBuffer(sizeof(FVertex) * MaxVertices, sizeof(FVertex));

	CB = RenderCommand::CreateConstantBuffer(sizeof(FMatrix));

	return true;
}

void FLineBatcher::BuildVertexBuffer()
{
	for (UPrimitiveComponent* Prim : World->GetPrimitiveComponents())
	{
		const FTransform& Transform = Prim->GetTransform();
		FBox WorldBox = Prim->GetMeshData().AABB.GetWorldAABB(Transform.GetLocalMatrix());
		AddBox(WorldBox, FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	}
}

void FLineBatcher::BeginFrame()
{
	VertexBufferPtr = VertexBufferBase;
	VertexCount = 0;
}

void FLineBatcher::AddLine(const FVector& A, const FVector& B, const FVector4& Color)
{
	if (VertexCount + 2 > MaxVertices)
		return;   // 넘치면 버림

	*VertexBufferPtr++ = { A, Color };
	*VertexBufferPtr++ = { B, Color };

	VertexCount += 2;
}

void FLineBatcher::AddBox(FBox BoundingBox, FVector4 Color)
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

void FLineBatcher::AddPath(const TArray<FVector>& Points, const FVector4& Color)
{
	if (Points.IsEmpty())
	{
		return;
	}
	for (size_t i = 0; i < Points.size() - 1; ++i) {
		AddLine(Points[i], Points[i + 1], Color);
	}
}
void FLineBatcher::Flush()
{
	RenderCommand::BindShader(LineShader.get());
	RenderCommand::BindVertexBuffer(VertexBuffer.get());
	RenderCommand::BindConstantBuffer(0, CB.get(), EShaderBindFlagBits::Vertex);

	RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	RenderCommand::Draw(VertexCount, 0);
}

void FLineBatcher::OnRender(const FMatrix& ViewProjection)
{
	//BuildVertexBuffer();

	FMatrix VP = ViewProjection.GetTransposed();
	RenderCommand::UpdateBufferData(VertexBuffer.get(), VertexBufferBase, sizeof(FVertex) * VertexCount);
	RenderCommand::UpdateBufferData(CB.get(), &VP, sizeof(FMatrix));

	Flush();
}


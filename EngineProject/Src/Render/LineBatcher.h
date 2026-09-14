#pragma once

#include "Buffer.h"
#include "StaticMeshData.h"

class FLineBatcher
{
public:
	FLineBatcher() = default;
	~FLineBatcher();
	bool Init(FRenderer* InRenderer, UWorld* InWorld);
	void BuildVertexBuffer();

	void BeginFrame();


	void AddLine(const FVector& A, const FVector& B, const FVector4& Color);
	void AddBox(FBox BoundingBox, FVector4 Color);
	void AddPath(const TArray<FVector>& Points, const FVector4& Color);

	void Flush();
	void OnRender(const FMatrix& ViewProjection);

private:
	FRenderer* Renderer;

	UWorld* World;

	TSharedPtr<FShader> LineShader;
	FVertex* VertexBufferBase;  
	FVertex* VertexBufferPtr;  
	TSharedPtr<FConstantBuffer> CB;
	uint32 VertexCount;

	uint32 MaxVertices = 0;
	TSharedPtr<FVertexBuffer> VertexBuffer;
};
#pragma once

#include "Buffer.h"
#include "StaticMeshData.h"

class FDebugRenderer
{
public:
	FDebugRenderer() = default;
	bool Init(FRenderer* InRenderer, UWorld* InWorld);
	void BuildVertexBuffer();

	void AddBox(FBox BoundingBox);
	void Flush();

private:
	FRenderer* Renderer;
	UWorld* World;

	FVertex* Vertices;  

	uint32 MaxVertices = 0;
	TSharedPtr<FVertexBuffer> VertexBuffer;
};
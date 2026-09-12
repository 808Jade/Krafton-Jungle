#include "DebugRenderer.h"
#include "EnginePCH.h"



bool FDebugRenderer::Init(FRenderer* InRenderer, UWorld* InWorld)
{
	Renderer = InRenderer;
	World = InWorld;

	MaxVertices = 1000000;

	VertexBuffer = Renderer->CreateDynamicVertexBuffer(sizeof(FTextVertex) * MaxVertices, sizeof(FTextVertex));

	return false;
}

void FDebugRenderer::BuildVertexBuffer()
{
	//for (UPrimitiveComponent* Prim : World->GetPrimitiveComponents())
	//{
	//	AddBox(Prim->GetMeshData().AABB);
	//}
	//Flush(ViewProj);
}

void FDebugRenderer::AddBox(FBox BoundingBox)
{
	
}

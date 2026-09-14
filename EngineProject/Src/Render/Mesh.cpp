#include "EnginePCH.h"
#include "Mesh.h"
#include "Renderer.h"

UStaticMesh::~UStaticMesh()
{
	VertexBuffer = nullptr;
	IndexBuffer = nullptr;
}

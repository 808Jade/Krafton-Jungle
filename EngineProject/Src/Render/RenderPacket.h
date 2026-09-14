#pragma once
#include "Math/Transform.h"
#include "Mesh.h"
#include "Shader.h"

// Todo: subuv
class UTexture2D;

struct FRenderPacket {
	FMatrix model;
	UStaticMesh* mesh;
	FShader* shader;

	// Todo: subuv
	bool bSubUV;

	UTexture2D* texture;
	uint32 CurrentFrame;
	uint32 AtlasColumns;
	uint32 AtlasRows;
};
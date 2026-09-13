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
	UTexture2D* texture = nullptr;
	bool bSubUV = false;
	uint32 CurrentFrame = 0;
	uint32 AtlasColumns = 1;
	uint32 AtlasRows = 1;
};
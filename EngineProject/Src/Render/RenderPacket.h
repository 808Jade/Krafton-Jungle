#pragma once
#include "Math/Transform.h"
#include "Mesh.h"
#include "Shader.h"

struct FRenderPacket {
	FMatrix model;
	UStaticMesh* mesh;
	FShader* shader;
};
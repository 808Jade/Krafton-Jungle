#pragma once
#include "Math/Transform.h"
#include "Mesh.h"
#include "Shader.h"

struct FRenderPacket {
	FMatrix model;
	FMesh* mesh;
	FShader* shader;
};
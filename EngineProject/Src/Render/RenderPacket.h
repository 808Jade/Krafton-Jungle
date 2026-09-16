#pragma once
#include "Math/Transform.h"
#include "Mesh.h"
#include "Shader.h"
#include "Material.h"

// Todo: subuv
class UTexture2D;

struct FRenderPacket {
	FMatrix model;
	UStaticMesh* mesh;
	UMaterial* material;
};
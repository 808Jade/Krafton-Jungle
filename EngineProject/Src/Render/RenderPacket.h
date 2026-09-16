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

	// 카메라와의 거리 제곱. 반투명 정렬에 사용
	float CameraToParticleDistance = 0.0f;
};
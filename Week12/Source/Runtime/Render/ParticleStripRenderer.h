#pragma once

#include "Render/Buffer.h"
#include "Particles/ParticleStripData.h"
#include "Particles/ParticleVertex.h"

class UMaterial;
class FShaderProgram;

// 현재 카메라 정보 구조를 재사용한다.
// 이후 Sprite/Strip 공통 FParticleView로 이름을 바꿔도 된다.
struct FParticleSpriteView;

struct alignas(16) FParticleStripConstants
{
	FMatrix ViewProjection;
	FVector4 MaterialColor;

	uint32 UseTexture = 0;
	float Padding[3] = {};
};

class FParticleStripRenderer
{
public:
	uint64 GetAllocatedBufferBytes() const
	{
		uint64 Bytes = 0;
		if (VertexBuffer && VertexBuffer->GetBuffer()) Bytes += VertexBuffer->GetBufferSize();
		if (IndexBuffer && IndexBuffer->GetBuffer()) Bytes += IndexBuffer->GetBufferSize();
		if (ConstantsBuffer && ConstantsBuffer->GetBuffer()) Bytes += ConstantsBuffer->GetBufferSize();
		return Bytes;
	}

	bool Init();

	bool Draw(
	    const TArray<FParticleStripPoint>& Points,
	    UMaterial* Material,
	    const FParticleSpriteView& View);

private:
	bool EnsureCapacity(uint32 RequiredPointCount);

	void BuildVertices(
	    const TArray<FParticleStripPoint>& Points,
	    const FParticleSpriteView& View);

private:
	TUniquePtr<FVertexBuffer> VertexBuffer;
	TUniquePtr<FIndexBuffer> IndexBuffer;
	TUniquePtr<FConstantBuffer> ConstantsBuffer;

	FShaderProgram* Shader = nullptr;
	ComPtr<ID3D11InputLayout> InputLayout;

	uint32 PointCapacity = 0;

	// Draw마다 재사용하는 CPU 작업 배열.
	TArray<FParticleStripPoint> CleanPoints;
	TArray<FParticleStripVertex> Vertices;
};
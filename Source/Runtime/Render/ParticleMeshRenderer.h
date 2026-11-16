#pragma once

#include "Render/Buffer.h"
#include "Particles/ParticleVertex.h"

class FShaderProgram;
struct FRenderPacket;

struct alignas(16) FParticleMeshConstants
{
	FMatrix ViewProjection;
	FVector4 BaseColor;

	uint32 bOpaque = 1;
	uint32 Padding[3] = {};
};

class FParticleMeshRenderer
{
public:
	bool Init();
	
	bool Draw(
	    const FRenderPacket& Packet,
	    const FMatrix& ViewProjection,
	    bool bDepthOnly);

private:
	bool EnsureCapacity(uint32 RequiredCount);

	TUniquePtr<FVertexBuffer> InstanceVB;
	TUniquePtr<FConstantBuffer> ConstantsCB;

	FShaderProgram* Shader = nullptr;
	ComPtr<ID3D11InputLayout> InputLayout;

	uint32 InstanceCapacity = 0;
};

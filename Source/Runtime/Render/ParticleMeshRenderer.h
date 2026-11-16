#pragma once

#include "Render/Buffer.h"
#include "Render/ViewMode.h"
#include "Particles/ParticleVertex.h"

class FShaderProgram;
struct FRenderPacket;

struct alignas(16) FParticleMeshBatchConstants
{
	uint32 ReceiveShadow = 0;
	uint32 ReceiveDecal = 0;
	uint32 Padding[2] = {};
};

class FParticleMeshRenderer
{
public:
	bool Init();

	bool Draw(
	    const FRenderPacket& Packet,
	    EViewMode ViewMode,
	    bool bDepthOnly);

private:
	bool EnsureCapacity(uint32 RequiredCount);

	TUniquePtr<FVertexBuffer> InstanceVB;

	// UberLit b1, b3.
	TUniquePtr<FConstantBuffer> MaterialCB;
	TUniquePtr<FConstantBuffer> BatchCB;

	ComPtr<ID3D11InputLayout> InputLayouts[
		static_cast<int32>(EViewMode::Count)];

	uint32 InstanceCapacity = 0;
};

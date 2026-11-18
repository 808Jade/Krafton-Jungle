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
	uint64 GetAllocatedBufferBytes() const
	{
		uint64 Bytes = 0;
		if (InstanceVB && InstanceVB->GetBuffer()) Bytes += InstanceVB->GetBufferSize();
		if (MaterialCB && MaterialCB->GetBuffer()) Bytes += MaterialCB->GetBufferSize();
		if (BatchCB && BatchCB->GetBuffer()) Bytes += BatchCB->GetBufferSize();
		return Bytes;
	}

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

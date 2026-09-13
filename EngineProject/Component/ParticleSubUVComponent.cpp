#include "EnginePCH.h"
#include "Component/ParticleSubUVComponent.h"
#include "Asset/AssetManager.h"

// Todo: subuv
void UParticleSubUVComponent::BeginPlay()
{
	Super::BeginPlay();

	AtlasTexturePath = "ParticleAtlas.png";
	ColSize = 4;
	RowSize = 4;
	CurrentFrame = 0;
	FrameTimer = 0.0f;
	FrameRate = 12.0f;
	AtlasTexture = nullptr;

	Shader = UAssetManager::Get().GetShader("Shader/ParticleSubUVShader.hlsl");
	Mesh = UAssetManager::Get().GetMesh("ParticleQuad");
	AtlasTexture = UAssetManager::Get().GetTexture(AtlasTexturePath);

	Type = EPrimitiveType::Particle;
}

// Todo: subuv
void UParticleSubUVComponent::SetSubUVSize(uint32 NewColSize, uint32 NewRowSize)
{
	assert(NewColSize > 0 && NewRowSize > 0);

	ColSize = NewColSize;
	RowSize = NewRowSize;

	/*
	const uint32 TotalFrames = ColSize * RowSize;
	if (CurrentFrame >= TotalFrames)
	{
		CurrentFrame = 0;
	}
	*/
}

// Todo: subuv
void UParticleSubUVComponent::TickComponent(float DeltaTime)
{
	FrameTimer += DeltaTime;

	const float FrameDuration = 1.0f / FrameRate;
	if (FrameTimer < FrameDuration)
	{
		return;
	}

	FrameTimer = 0.0f;

	const uint32 TotalFrames = ColSize * RowSize;
	++CurrentFrame;
	CurrentFrame %= TotalFrames;
}

// Todo: subuv
void UParticleSubUVComponent::SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue)
{
	assert(Mesh != nullptr);
	assert(Shader != nullptr);
	assert(AtlasTexture != nullptr);

	FRenderPacket Packet;
	Packet.model = GetWorldMatrix();
	Packet.mesh = Mesh;
	Packet.shader = Shader;
	Packet.texture = AtlasTexture;
	Packet.bSubUV = true;
	Packet.CurrentFrame = CurrentFrame;
	Packet.AtlasColumns = ColSize;
	Packet.AtlasRows = RowSize;

	RenderQueue.Enqueue(Packet);
}

#include "EnginePCH.h"
#include "Component/ParticleSubUVComponent.h"
#include "Asset/AssetManager.h"

// Todo: subuv
void UParticleSubUVComponent::BeginPlay()
{
	Super::BeginPlay();

	AtlasTexturePath = "ParticleAtlas.png";
	NumCols = 4;
	NumRows = 4;
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
void UParticleSubUVComponent::SetSubUVSize(uint32 InCols, uint32 InRows)
{
	NumCols = (std::max)(1u, InCols);
	NumRows = (std::max)(1u, InRows);

	const uint32 TotalFrames = NumCols * NumRows;
	if (CurrentFrame >= TotalFrames)
	{
		CurrentFrame = 0;
	}
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

	FrameTimer -= FrameDuration;

	++CurrentFrame;

	const uint32 TotalFrames = NumCols * NumRows;
	if (CurrentFrame >= TotalFrames)
	{
		CurrentFrame = 0;
	}
}

// Todo: subuv
void UParticleSubUVComponent::SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue)
{
	if (!Mesh || !Shader || !AtlasTexture)
	{
		return;
	}

	FRenderPacket Packet;
	Packet.model = GetWorldMatrix();
	Packet.mesh = Mesh;
	Packet.shader = Shader;
	// Todo: subuv
	Packet.texture = AtlasTexture;
	// Todo: subuv
	Packet.bSubUV = true;
	// Todo: subuv
	Packet.CurrentFrame = CurrentFrame;
	// Todo: subuv
	Packet.AtlasColumns = NumCols;
	// Todo: subuv
	Packet.AtlasRows = NumRows;

	RenderQueue.Enqueue(Packet);
}

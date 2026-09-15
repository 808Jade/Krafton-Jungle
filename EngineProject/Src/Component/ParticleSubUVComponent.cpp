#include "EnginePCH.h"
#include "Component/ParticleSubUVComponent.h"
#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"

// Todo: subuv
void UParticleSubUVComponent::BeginPlay()
{
	Super::BeginPlay();

	// Todo: Default setting, set value from arguments
	//AtlasTexturePath = "ParticleAtlas.png";
	AtlasTexturePath = "CloudAtlas.png";

	ColSize = 8;
	RowSize = 8;
	CurrentFrame = 0;
	FrameTimer = 0.0f;
	FrameRate = 12.0f;
	AtlasTexture = nullptr;

	Shader = RenderCommand::CreateShader(L"Assets/Shader/ParticleSubUVShader.hlsl", FParticleVertex::GetLayout());
	Mesh = UAssetManager::GetAssetByPath<UStaticMesh>("ParticleQuad");
	AtlasTexture = UAssetManager::GetAssetByPath<UTexture2D>(AtlasTexturePath);

	Type = EPrimitiveType::Particle;
}

void UParticleSubUVComponent::SetAtlasTexturePath(const FString& InPath)
{
	AtlasTexturePath = InPath;
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

void UParticleSubUVComponent::SetFrameRate(float InFrameRate)
{
	FrameRate = (InFrameRate > 0.0f) ? InFrameRate : 1.0f;
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
	Packet.shader = Shader.get();

	Packet.bSubUV = true;
	Packet.texture = AtlasTexture;
	Packet.CurrentFrame = CurrentFrame;
	Packet.AtlasColumns = ColSize;
	Packet.AtlasRows = RowSize;

	RenderQueue.Enqueue(Packet);
}

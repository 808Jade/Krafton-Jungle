#include "EnginePCH.h"
#include "Component/ParticleSubUVComponent.h"
#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

#include "BillboardComponent.h"

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

void UParticleSubUVComponent::SetSubUVSize(uint32 NewColSize, uint32 NewRowSize)
{
	assert(NewColSize > 0 && NewRowSize > 0);

	ColSize = NewColSize;
	RowSize = NewRowSize;
}

void UParticleSubUVComponent::SetFrameRate(float InFrameRate)
{
	FrameRate = (InFrameRate > 0.0f) ? InFrameRate : 1.0f;
}

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

void UParticleSubUVComponent::SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue)
{
	assert(Mesh != nullptr);
	assert(Shader != nullptr);
	assert(AtlasTexture != nullptr);

	FMatrix WorldMatrix;
	UBillboardComponent::GetWorldTransformedMatrix(&WorldMatrix);

	UCameraComponent* CameraComponent = GetOwner()->GetWorld()->GetMainCamera()->GetCameraComponent();
	assert(CameraComponent != nullptr);

	// Calculate camera to particle distance
	const FVector CameraPos = CameraComponent->GetWorldLocation();
	const FVector ParticlePos = GetWorldLocation();

	const FVector CameraToParticleVec = ParticlePos - CameraPos;
	const float CameraToParticleDistance =
		CameraToParticleVec.X * CameraToParticleVec.X +
		CameraToParticleVec.Y * CameraToParticleVec.Y +
		CameraToParticleVec.Z * CameraToParticleVec.Z;

	// Set RenderPacket
	FRenderPacket Packet;
	Packet.model = WorldMatrix;
	Packet.mesh = Mesh;
	Packet.shader = Shader.get();

	Packet.bSubUV = true;
	Packet.texture = AtlasTexture;
	Packet.CurrentFrame = CurrentFrame;
	Packet.AtlasColumns = ColSize;
	Packet.AtlasRows = RowSize;

	Packet.CameraToParticleDistance = CameraToParticleDistance;

	RenderQueue.Enqueue(Packet);
}
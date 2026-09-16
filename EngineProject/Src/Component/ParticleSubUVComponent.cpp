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

	ColSize = 8;
	RowSize = 8;
	CurrentFrame = 0;
	FrameTimer = 0.0f;
	FrameRate = 12.0f;
	AtlasTexture = nullptr;

	//Shader = RenderCommand::CreateShader(L"Resources/Shader/ParticleSubUVShader.hlsl", FParticleVertex::GetLayout());
	Mesh = UAssetManager::GetAssetByPath<UStaticMesh>("ParticleQuad");
	Material = UAssetManager::GetAssetByPath<UMaterial>("SubUVMaterial");

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
	Super::TickComponent(DeltaTime);
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
	assert(Material != nullptr);

	UCameraComponent* CameraComponent = GetOwner()->GetWorld()->GetMainCamera()->GetCameraComponent();
	assert(CameraComponent != nullptr);

	const FTransform& Transform = CameraComponent->GetTransform();

	FVector BbUp = Transform.GetUp().Normalized();                         // Z-up 월드
	FVector BbRight = FVector::Cross(BbUp, Transform.GetForward()).Normalized();
	FVector BbFwd = FVector::Cross(BbUp, BbRight);          // 자기모순 없게 외적으로 유도

	FMatrix World;
	World.SetIdentity();
	// row0 = 로컬 +X = up
	World.M[0][0] = BbUp.X;    World.M[0][1] = BbUp.Y;    World.M[0][2] = BbUp.Z;    World.M[0][3] = 0;
	// row1 = 로컬 +Y = right
	World.M[1][0] = BbRight.X; World.M[1][1] = BbRight.Y; World.M[1][2] = BbRight.Z; World.M[1][3] = 0;
	// row2 = 로컬 +Z = forward
	World.M[2][0] = BbFwd.X;         World.M[2][1] = BbFwd.Y;         World.M[2][2] = BbFwd.Z;         World.M[2][3] = 0;
	World.M[3][0] = GetWorldLocation().X;      World.M[3][1] = GetWorldLocation().Y;      World.M[3][2] = GetWorldLocation().Z;      World.M[3][3] = 1;

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
	Packet.model = World;
	Packet.mesh = Mesh;
	Packet.material = Material;

			/*Packet.bSubUV = true;
			Packet.texture = AtlasTexture;
			Packet.CurrentFrame = CurrentFrame;
			Packet.AtlasColumns = ColSize;
			Packet.AtlasRows = RowSize;*/

	FSubUVConstants subUVConstans;
	subUVConstans.CurrentFrame = CurrentFrame;
	subUVConstans.AtlasColSize = ColSize;
	subUVConstans.AtlasRowSize = RowSize;

	RenderCommand::UpdateBufferData(Packet.material->ParamBuffer.get(), &subUVConstans);

	Packet.CameraToParticleDistance = CameraToParticleDistance;

	RenderQueue.Enqueue(Packet);
}
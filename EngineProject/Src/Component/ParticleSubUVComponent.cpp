#include "EnginePCH.h"
#include "Component/ParticleSubUVComponent.h"
#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"


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

	//Shader = RenderCommand::CreateShader(L"Resources/Shader/ParticleSubUVShader.hlsl", FParticleVertex::GetLayout());
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

	// TextRenderer와 동일하게 Camera Transform 획득
	AActor* Owner = GetOwner();
	assert(Owner != nullptr);

	UWorld* World= Owner->GetWorld();
	assert(World!= nullptr);

	ACameraActor* Camera = World->GetMainCamera();
	assert(Camera != nullptr);

	UCameraComponent* CameraComponent = Camera->GetCameraComponent();
	assert(CameraComponent != nullptr);

	const FTransform& Transform = CameraComponent->GetTransform();

	// TextRenderer와 동일
	FVector Right = Transform.GetRight().Normalized();
	FVector Up = Transform.GetUp().Normalized();
	FVector Forward = Transform.GetForward().Normalized();

	FVector BbUp = Transform.GetUp().Normalized();
	FVector BbRight = FVector::Cross(BbUp, Forward).Normalized();
	FVector BbFwd = FVector::Cross(BbUp, BbRight);

	if (BbRight.Length() <= 1e-6f)
	{
		BbRight = Right;
		BbFwd = FVector::Cross(BbUp, BbRight);
	}

	const FVector WorldPos = GetWorldLocation();
	const FVector WorldScale = GetWorldScale3D();

	FMatrix WorldMatrix;
	WorldMatrix.SetIdentity();

	// Y -> Billboard Right
	WorldMatrix.M[0][0] = -BbRight.X;
	WorldMatrix.M[0][1] = -BbRight.Y;
	WorldMatrix.M[0][2] = -BbRight.Z;
	WorldMatrix.M[0][3] = 0.0f;

	// Z -> Billboard Up
	WorldMatrix.M[1][0] = BbUp.X;
	WorldMatrix.M[1][1] = BbUp.Y;
	WorldMatrix.M[1][2] = BbUp.Z;
	WorldMatrix.M[1][3] = 0.0f;

	// X -> Billboard Forward
	WorldMatrix.M[2][0] = -BbFwd.X;
	WorldMatrix.M[2][1] = -BbFwd.Y;
	WorldMatrix.M[2][2] = -BbFwd.Z;
	WorldMatrix.M[2][3] = 0.0f;

	// Position
	WorldMatrix.M[3][0] = WorldPos.X;
	WorldMatrix.M[3][1] = WorldPos.Y;
	WorldMatrix.M[3][2] = WorldPos.Z;
	WorldMatrix.M[3][3] = 1.0f;

	FRenderPacket Packet;
	Packet.model = WorldMatrix;
	Packet.mesh = Mesh;
	Packet.material = Material;

	/*Packet.bSubUV = true;
	Packet.texture = AtlasTexture;
	Packet.CurrentFrame = CurrentFrame;
	Packet.AtlasColumns = ColSize;
	Packet.AtlasRows = RowSize;*/

	RenderQueue.Enqueue(Packet);
}
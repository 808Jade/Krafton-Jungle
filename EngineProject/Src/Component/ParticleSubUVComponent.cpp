#include "EnginePCH.h"
#include "Component/ParticleSubUVComponent.h"
#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

#include "BillboardComponent.h"

const float UParticleSubUVComponent::MAX_NORMALIZED_VALUE = 1.0f;
const float UParticleSubUVComponent::FADE_START = 0.7;

void UParticleSubUVComponent::BeginPlay()
{
	Super::BeginPlay();

	// Todo: Default setting, set value from arguments
	//AtlasTexturePath = "ParticleAtlas.png";

	ColSize = 8;
	RowSize = 8;
	FrameTimer = 0.0f;++
	FrameRate = 12.0f;
	// AtlasTexture = nullptr;

	//Shader = RenderCommand::CreateShader(L"Resources/Shader/ParticleSubUVShader.hlsl", FParticleVertex::GetLayout());
	Mesh = UAssetManager::GetAssetByPath<UStaticMesh>("ParticleQuad");
	Material = UAssetManager::GetAssetByPath<UMaterial>("SubUVMaterial");

	Type = EPrimitiveType::Particle;
	
	for (uint8 i = 0; i < MAX_PARTICLE_COMPONENTS_COUNT; ++i)
	{
		FParticle Particle;
		RespawnParticle(Particle);

		Particle.Age = GetRandomNumberBetween(0.0f, Particle.LifeTime);
		Particle.Location.Z += Particle.Velocity.Z * Particle.Age;

		const float LifeRatio = Particle.Age / Particle.LifeTime;
		Particle.Scale = Lerp(0.4f, 3.0f, LifeRatio);
		
		const float ElapsedFrames = Particle.Age * FrameRate;
		Particle.SubUVFrame = static_cast<uint32>(ElapsedFrames) % (ColSize * RowSize);

		Particles.Add(Particle);
	}
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
	const uint32 TotalFrames = ColSize * RowSize;

	for (FParticle& Particle : Particles)
	{
		Particle.Location += Particle.Velocity * DeltaTime;
		Particle.Age += DeltaTime;

		if (Particle.Age >= Particle.LifeTime)
		{
			RespawnParticle(Particle);

			continue;
		}

		const float LifeRatio = Particle.Age / Particle.LifeTime;
		
		Particle.Scale = Lerp(0.5f, 3.0f, LifeRatio);

		if (LifeRatio > FADE_START)
		{
			const float FadeRatio = GedSmoothStepedRatio(FADE_START, MAX_NORMALIZED_VALUE, LifeRatio);
			Particle.Alpha = MAX_NORMALIZED_VALUE - FadeRatio;
		}
		else
		{
			Particle.Alpha = MAX_NORMALIZED_VALUE;
		}

		Particle.FrameTimer += DeltaTime;
		const float FrameDuration = MAX_NORMALIZED_VALUE / FrameRate;

		if (Particle.FrameTimer >= FrameDuration)
		{
			Particle.FrameTimer = 0.0f;

			++Particle.SubUVFrame;
			Particle.SubUVFrame %= TotalFrames;
		}
	}
}

void UParticleSubUVComponent::SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue)
{
	assert(Mesh != nullptr);
	assert(Material != nullptr);
	
	Constants.Reset();
	Constants.Reserve(Particles.Num());

	const FVector CameraPos = GetOwner()->GetWorld()->GetMainCamera()->GetCameraComponent()->GetWorldLocation();
	for (FParticle& Particle : Particles)
	{
		if (Particle.bAlive == false)
		{
			continue;
		}


		FMatrix WorldMatrix = FMatrix::Identity; 
		Super::GetWorldTransformedMatrix(&WorldMatrix);

		// Scale, Move
		WorldMatrix.M[0][0] *= Particle.Scale;
		WorldMatrix.M[0][1] *= Particle.Scale;
		WorldMatrix.M[0][2] *= Particle.Scale;
		WorldMatrix.M[0][3] *= Particle.Scale;

		WorldMatrix.M[1][0] *= Particle.Scale;
		WorldMatrix.M[1][1] *= Particle.Scale;
		WorldMatrix.M[1][2] *= Particle.Scale;
		WorldMatrix.M[1][3] *= Particle.Scale;

		WorldMatrix.M[2][0] *= Particle.Scale;
		WorldMatrix.M[2][1] *= Particle.Scale;
		WorldMatrix.M[2][2] *= Particle.Scale;
		WorldMatrix.M[2][3] *= Particle.Scale;

		WorldMatrix.M[3][0] = Particle.Location.X;
		WorldMatrix.M[3][1] = Particle.Location.Y;
		WorldMatrix.M[3][2] = Particle.Location.Z;
		WorldMatrix.M[3][3] = 1.0f;

		const FVector ParticlePos = Particle.Location;
		const FVector CameraToParticleVec = ParticlePos - CameraPos;

		const float CameraToParticleDistance = 
			CameraToParticleVec.X *
			CameraToParticleVec.X +

			CameraToParticleVec.Y *
			CameraToParticleVec.Y +

			CameraToParticleVec.Z *
			CameraToParticleVec.Z;

		FRenderPacket Packet;
		Packet.model = WorldMatrix;
		Packet.mesh = Mesh;
		Packet.material = Material;

		FSubUVConstants C;
		C.CurrentFrame =  Particle.SubUVFrame;
		C.AtlasColSize = ColSize;
		C.AtlasRowSize = RowSize;
		C.Alpha = Particle.Alpha;

		Constants.Add(C);

		Packet.Data = &Constants.Last();
		Packet.DataSize = sizeof(FSubUVConstants);

		/*Packet.texture = AtlasTexture;

		Packet.CurrentFrame = Particle.SubUVFrame;
		Packet.AtlasColumns = ColSize;

		Packet.AtlasRows = RowSize;
		Packet.ParticleAlpha = Particle.Alpha;*/
		Packet.CameraToParticleDistance = CameraToParticleDistance;

		RenderQueue.Enqueue(Packet);
	}
}

void UParticleSubUVComponent::RespawnParticle(FParticle& Particle)
{
	Particle.Location = GetWorldLocation();

	Particle.Velocity.X = 0.0f;
	Particle.Velocity.Y = 0.0f;
	Particle.Velocity.Z = GetRandomNumberBetween(0.5f, 0.8f);

	Particle.LifeTime = GetRandomNumberBetween(3.5f, 10.0f);
	Particle.Age = 0.0f;
	Particle.SubUVFrame = static_cast<uint32>(GetRandomNumberBetween(0.0f, static_cast<float>((ColSize * RowSize) - 1)));
	Particle.Scale = 0.4f;
	Particle.FrameTimer = 0.0f;
	Particle.bAlive = true;

	Particle.Alpha = MAX_NORMALIZED_VALUE;
}

// Todo: Move to util class
float UParticleSubUVComponent::GetRandomNumberBetween(float start, float end) const
{
	float randomNumber = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

	return start + randomNumber * (end - start);
}

float UParticleSubUVComponent::Lerp(float left, float right, float alpha) const
{
	//assert(alpha >= 0.0f && alpha <= 1.0f);
	if (alpha < 0.0f)
	{
		alpha = 0.0f;
	}
	
	if (alpha > 1.0f)
	{
		alpha = 1.0f;
	}

	return left + (right - left) * alpha;
}

float UParticleSubUVComponent::GedSmoothStepedRatio(float start, float end, float value) const
{
	float normedValue = (value - start) / (end - start);
	//assert(normedValue >= 0.0f && normedValue <= 1.0f);

	if (normedValue < 0.0f)
	{
		normedValue = 0.0f;
	}

	if (normedValue > 1.0f)
	{
		normedValue = 1.0f;
	}

	return normedValue * normedValue * (3.0f - 2.0f * normedValue);
}
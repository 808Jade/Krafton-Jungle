#include "EnginePCH.h"
#include "ParticleSystemComponent.h"
#include "Engine/ParticleSystemSceneProxy.h"

#include "Asset/AssetManager.h"
#include "Render/Mesh.h"
#include "Render/Material.h"
#include "Math/Color.h"

#include <cassert>

UParticleSystemComponent::UParticleSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

UParticleSystemComponent::~UParticleSystemComponent()
{
	for (auto* EmitterInstance : EmitterInstances)
	{
		delete EmitterInstance;
	}
	EmitterInstances.Empty();
}

void UParticleSystemComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	if (bMeshInstancingTest)
	{
		if (EmitterInstances.IsEmpty())
		{
			InitializeMeshInstancingTest();
		}

		// 일반 Tick의 Spawn/Age/Integrate는 실행하지 않는다.
		for (auto* Emitter : EmitterInstances)
		{
			Emitter->UpdateTransforms();
		}

		return;
	}

	if (EmitterInstances.IsEmpty())
	{
		FParticleEmitterInstance* ParticleEmitterInstance = new FParticleSpriteEmitterInstance();
		ParticleEmitterInstance->Component = this;
		ParticleEmitterInstance->Init(100, sizeof(FBaseParticle));
		EmitterInstances.Add(ParticleEmitterInstance);
	}
	
	for (auto* EmitterInstance : EmitterInstances)
	{
		EmitterInstance->Tick(DeltaTime);
	}

	FPrimitiveSceneProxy* Proxy = GetSceneProxy();
	FBox NewBox{};
	if (!bUseFixedRelativeBoundingBox && Proxy && GetEmitterBoundingBox(NewBox))
	{
		const FAABB& Current = Proxy->GetBounds();
		const FVector CurrentMin = Current.Center - Current.Extent;
		const FVector CurrentMax = Current.Center + Current.Extent;

		const bool bInside =
			NewBox.Min.X >= CurrentMin.X && NewBox.Min.Y >= CurrentMin.Y && NewBox.Min.Z >= CurrentMin.Z &&
			NewBox.Max.X <= CurrentMax.X && NewBox.Max.Y <= CurrentMax.Y && NewBox.Max.Z <= CurrentMax.Z;

		if (!bInside)
		{
			OnTransformDirty();
		}
	}
}

FBox UParticleSystemComponent::CalcBounds() const
{
	if (bUseFixedRelativeBoundingBox)
	{
		return FixedRelativeBoundingBox.GetWorldAABB(GetWorldMatrix());
	}

	FBox BoundingBox{};
	if (!GetEmitterBoundingBox(BoundingBox))
	{
		const FVector Origin = GetWorldLocation();
		return FBox{ Origin, Origin };
	}

	const FVector ExpandAmount = (BoundingBox.Max - BoundingBox.Min) * 0.05f;
	return FBox{ BoundingBox.Min - ExpandAmount, BoundingBox.Max + ExpandAmount };
}

bool UParticleSystemComponent::GetEmitterBoundingBox(FBox& OutBoundingBox) const
{
	bool bValid = false;
	for (const FParticleEmitterInstance* EmitterInstance : EmitterInstances)
	{
		if (!EmitterInstance || EmitterInstance->ActiveParticles <= 0)
		{
			continue;
		}

		if (!bValid)
		{
			OutBoundingBox = EmitterInstance->GetBoundingBox();
			bValid = true;
		}
		else
		{
			OutBoundingBox.Expand(EmitterInstance->GetBoundingBox());
		}
	}
	return bValid;
}

void UParticleSystemComponent::InitializeMeshInstancingTest()
{
	UStaticMesh* Mesh = UAssetManager::GetAssetByPath<UStaticMesh>("Models/Nijika/Nijika.obj");

	if (!Mesh)
	{
		assert(false);
		return;
	}

	auto* Emitter = new FParticleMeshEmitterInstance();

	Emitter->Component = this;
	Emitter->TestMesh = Mesh;

	UMaterial* BaseMaterial = Mesh->GetMaterial(0);

	if (BaseMaterial)
	{
		UMaterial* TestMaterial =
		    UMaterial::CreateInstance(BaseMaterial);

		TestMaterial->SetBlendState(EBlendState::Opaque);

		// RGB는 기존 색 유지, alpha만 반투명으로 설정.
		//TestMaterial->BaseColor.W = 0.35f;

		Emitter->CurrentMaterial = TestMaterial;
	}

	Emitter->bUseLocalSpace = true;

	constexpr int32 TestCount = 1000;
	Emitter->Init(TestCount, sizeof(FBaseParticle));

	if (!Emitter->ParticleData)
	{
		delete Emitter;
		return;
	}

	Emitter->UpdateTransforms();

	// 기존 스폰 함수를 이용해 메모리와 기본 필드를 초기화.
	Emitter->SpawnParticles(
	    TestCount,
	    0.0f,
	    0.0f,
	    FVector::ZeroVector,
	    FVector::ZeroVector);

	const FVector Positions[3] = {
		FVector(0.0f, -3.0f, 2.0f),
		FVector(0.0f, 0.0f, 2.0f),
		FVector(0.0f, 3.0f, 2.0f)
	};

	const FVector Sizes[3] = {
		FVector(1.0f, 1.0f, 1.0f),
		FVector(0.5f, 1.0f, 1.5f),
		FVector(1.5f, 0.5f, 0.75f)
	};

	const FLinearColor Colors[3] = {
		FLinearColor(1.0f, 0.0f, 0.0f, 1.0f),
		FLinearColor(0.0f, 1.0f, 0.0f, 1.0f),
		FLinearColor(0.0f, 0.0f, 1.0f, 1.0f)
	};

	for (int32 Index = 0; Index < TestCount; ++Index)
	{
		const uint16 Slot = Emitter->ParticleIndices[Index];

		auto* Particle = reinterpret_cast<FBaseParticle*>(
		    Emitter->ParticleData +
		    Slot * Emitter->ParticleStride);

		const int32 X = Index % 10;
		const int32 Y = (Index / 10) % 10;
		const int32 Z = Index / 100;

		Particle->Location = FVector(
		    X * 2.0f,
		    Y * 2.0f,
		    Z * 2.0f + 2.0f);

		Particle->OldLocation = Particle->Location;

		Particle->BaseSize = FVector(0.5f, 0.5f, 0.5f);
		Particle->Size = Particle->BaseSize;

		Particle->Rotation = Index * 0.05f;

		Particle->BaseColor = FLinearColor(
		    X / 9.0f,
		    Y / 9.0f,
		    Z / 9.0f,
		    1.0f);

		Particle->Color = Particle->BaseColor;
	}

	EmitterInstances.Add(Emitter);

	// 초기 테스트는 충분히 큰 고정 Bounds를 사용.
	bUseFixedRelativeBoundingBox = true;
	FixedRelativeBoundingBox = FBox{
		FVector(-5.0f, -5.0f, -5.0f),
		FVector(25.0f, 25.0f, 25.0f)
	};

	SetReceiveShadow(true);
	SetCastShadow(true);
	SetReceiveDecal(true);

	OnTransformDirty();
}

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	return new FParticleSystemSceneProxy(this);
}

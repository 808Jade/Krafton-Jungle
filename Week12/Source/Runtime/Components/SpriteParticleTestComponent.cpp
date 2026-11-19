#include "EnginePCH.h"
#include "SpriteParticleTestComponent.h"

#include "Asset/AssetManager.h"
#include "Engine/SpriteParticleSceneProxy.h"
#include "Render/Material.h"

#include <cassert>

USpriteParticleTestComponent::USpriteParticleTestComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	UMaterial* BaseMaterial =
	    UAssetManager::GetAssetByPath<UMaterial>("SubUVMaterial");

	UTexture2D* WhiteTexture =
	    UAssetManager::GetAssetByPath<UTexture2D>("WhiteTexture");

	assert(BaseMaterial);
	assert(WhiteTexture);

	// 공유 SubUVMaterial 자체를 수정하지 않는다.
	Material = UMaterial::CreateInstance(BaseMaterial);
	assert(Material);

	Material->Textures.Reset();
	Material->Textures.Add(WhiteTexture);

	Material->SetBlendState(EBlendState::AlphaBlend);
	Material->DepthStencilState = EDepthStencilState::ReadOnly;
	Material->SamplerState = ESamplerState::LinearClamp;
}

void USpriteParticleTestComponent::OnRegister()
{
	Super::OnRegister();

	if (TestParticles.Num() == 0)
	{
		ResetTestParticles(bUseLocalSpace);
	}
}

FPrimitiveSceneProxy*
USpriteParticleTestComponent::CreateSceneProxy()
{
	return new FSpriteParticleSceneProxy(this);
}

FBox USpriteParticleTestComponent::CalcLocalBounds() const
{
	// 테스트 파티클 전체를 포함하는 임시 Bounds.
	// Component 스케일은 (1, 1, 1)을 사용한다.
	FBox Bounds;
	Bounds.Min = FVector(-4.0f, -4.0f, -4.0f);
	Bounds.Max = FVector(4.0f, 4.0f, 4.0f);
	return Bounds;
}

FBox USpriteParticleTestComponent::CalcBounds() const
{
	if (TestParticleIndices.Num() == 0)
	{
		const FVector Origin = GetWorldLocation();
		return FBox{ Origin, Origin };
	}

	const FMatrix LocalToWorld = GetWorldMatrix();
	const FVector RenderScale = GetParticleRenderScale();

	FBox Result{};
	bool bInitialized = false;

	for (uint16 ParticleIndex : TestParticleIndices)
	{
		const FBaseParticle& Particle =
		    TestParticles[ParticleIndex];

		const FVector Center =
		    bUseLocalSpace
		        ? LocalToWorld.TransformPosition(Particle.Location)
		        : Particle.Location;

		const float Width =
		    std::abs(Particle.Size.X * RenderScale.X);

		const float Height =
		    std::abs(Particle.Size.Y * RenderScale.Y);

		// 전체 너비/높이 기준 반대각선 길이.
		const float Radius =
		    0.5f * std::sqrt(
		               Width * Width + Height * Height);

		const FVector Extent(Radius, Radius, Radius);

		const FBox ParticleBounds{
			Center - Extent,
			Center + Extent
		};

		if (!bInitialized)
		{
			Result = ParticleBounds;
			bInitialized = true;
		}
		else
		{
			Result.Expand(ParticleBounds);
		}
	}

	return Result;
}

FDynamicSpriteEmitterReplayData
USpriteParticleTestComponent::BuildReplayData() const
{
	FDynamicSpriteEmitterReplayData Replay;

	Replay.ActiveParticleCount = TestParticleIndices.Num();
	Replay.ParticleStride =
	    static_cast<int32>(sizeof(FBaseParticle));

	Replay.MaterialInterface = GetMaterial(0);
	Replay.bUseLocalSpace = bUseLocalSpace;
	Replay.Scale = GetParticleRenderScale();
	Replay.SortMode = eParticleSortMode;
	Replay.ScreenAlignment = eParticleScreenAlignment;
	Replay.AxisLock = eParticleAxisLock;

	if (Replay.ActiveParticleCount > 0)
	{
		Replay.DataContainer.CopyFrom(
		    reinterpret_cast<const uint8*>(
		        TestParticles.GetData()),
		    TestParticles.Num() *
		        static_cast<int32>(sizeof(FBaseParticle)),
		    TestParticleIndices.GetData(),
		    TestParticleIndices.Num());
	}

	return Replay;
}

void USpriteParticleTestComponent::ResetTestParticles(
    bool bInLocalSpace)
{
	bUseLocalSpace = bInLocalSpace;

	constexpr int32 SlotCount = 8;
	constexpr int32 ActiveCount = 3;

	TestParticles.SetNum(SlotCount);
	TestParticleIndices.SetNum(ActiveCount);

	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		TestParticles[Index] = FBaseParticle{};
	}

	const uint16 Slots[ActiveCount] = { 4, 1, 7 };

	// 컴포넌트의 로컬 X축을 따라 배치.
	const FVector Positions[ActiveCount] = {
		FVector(-1.0f, 0.0f, 0.0f),
		FVector(0.0f, 0.0f, 0.0f),
		FVector(1.0f, 0.0f, 0.0f)
	};

	const FLinearColor Colors[ActiveCount] = {
		FLinearColor(1.0f, 0.0f, 0.0f, 0.5f),
		FLinearColor(0.0f, 1.0f, 0.0f, 0.5f),
		FLinearColor(0.0f, 0.0f, 1.0f, 0.5f)
	};

	const FMatrix LocalToWorld = GetWorldMatrix();

	for (int32 ActiveIndex = 0;
	     ActiveIndex < ActiveCount;
	     ++ActiveIndex)
	{
		const uint16 Slot = Slots[ActiveIndex];
		TestParticleIndices[ActiveIndex] = Slot;

		FBaseParticle& Particle = TestParticles[Slot];

		Particle.Location =
		    bUseLocalSpace
		        ? Positions[ActiveIndex]
		        : LocalToWorld.TransformPosition(
		              Positions[ActiveIndex]);

		Particle.OldLocation = Particle.Location;

		// 정면에서 서로 겹치도록 같은 크기로 설정.
		Particle.Size = FVector(2.0f, 2.0f, 1.0f);
		Particle.BaseSize = Particle.Size;

		Particle.Rotation = 0.0f;

		Particle.Color = Colors[ActiveIndex];
		Particle.BaseColor = Particle.Color;

		Particle.Velocity = FVector(0.0f, 0.0f, 0.0f);
		Particle.BaseVelocity = Particle.Velocity;
	}

	OnTransformDirty();

	if (FPrimitiveSceneProxy* Proxy = GetSceneProxy())
	{
		Proxy->InvalidateDynamicData();
	}
}

FVector USpriteParticleTestComponent::GetParticleRenderScale() const
{
	const FVector Scale = GetWorldScale3D();

	return FVector(
	    std::abs(Scale.X),
	    std::abs(Scale.Y),
	    std::abs(Scale.Z));
}

void USpriteParticleTestComponent::SetScreenAlignment(
    EParticleScreenAlignment InAlignment)
{
	eParticleScreenAlignment = InAlignment;
	if (FPrimitiveSceneProxy* Proxy = GetSceneProxy())
	{
		Proxy->InvalidateDynamicData();
	}
}
void USpriteParticleTestComponent::SetAxisLock(
    EParticleAxisLock InAxisLock)
{
	eParticleAxisLock = InAxisLock;
	if (FPrimitiveSceneProxy* Proxy = GetSceneProxy())
	{
		Proxy->InvalidateDynamicData();
	}
}

void USpriteParticleTestComponent::SetSortMode(
    EParticleSortMode InSortMode)
{
	eParticleSortMode = InSortMode;
	if (FPrimitiveSceneProxy* Proxy = GetSceneProxy())
	{
		Proxy->InvalidateDynamicData();
	}
}

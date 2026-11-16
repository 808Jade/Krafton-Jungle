#include "EnginePCH.h"
#include "ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Engine/ParticleSystemSceneProxy.h"

UParticleSystemComponent::UParticleSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

UParticleSystemComponent::~UParticleSystemComponent()
{
	ResetParticles();
}

void UParticleSystemComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	if (Template && EmitterInstances.IsEmpty())
	{
		InitParticles();
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

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	return new FParticleSystemSceneProxy(this);
}

void UParticleSystemComponent::InitParticles()
{
	if (!Template)
	{
		return;
	}

	ResetParticles();

	for (auto* Emitter : Template->Emitters)
	{
		if (!Emitter)
		{
			continue;
		}
		Emitter->CacheEmitterModuleInfo();

		FParticleEmitterInstance* ParticleEmitterInstance = nullptr;
		if (Emitter->EmitterType == EDynamicEmitterType::Mesh)
		{
			ParticleEmitterInstance = new FParticleMeshEmitterInstance();
		}
		else
		{
			ParticleEmitterInstance = new FParticleSpriteEmitterInstance();
		}
		ParticleEmitterInstance->InitParameters(Emitter, this);
		EmitterInstances.Add(ParticleEmitterInstance);
	}
}

void UParticleSystemComponent::ResetParticles()
{
	for (auto* EmitterInstance : EmitterInstances)
	{
		delete EmitterInstance;
	}
	EmitterInstances.Empty();
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* InTemplate)
{
	ResetParticles();
	Template = InTemplate;
}

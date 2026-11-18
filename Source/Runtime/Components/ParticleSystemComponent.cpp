#include "EnginePCH.h"
#include "ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/Modules/ParticleModuleTypeDataBase.h"
#include "Engine/ParticleSystemSceneProxy.h"

#include "Asset/AssetManager.h"
#include "Render/Mesh.h"
#include "Render/Material.h"
#include "Math/Color.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/MinimalViewInfo.h"
#include "GameFramework/Actor.h"

#include <cassert>
#include <cmath>

namespace
{
	EDynamicEmitterType GetTemplateEmitterType(const UParticleEmitter* Emitter)
	{
		const auto* LOD = Emitter && !Emitter->LODLevels.IsEmpty() ? Emitter->LODLevels[0] : nullptr;
		return LOD && LOD->TypeDataModule ? LOD->TypeDataModule->GetEmitterType() : EDynamicEmitterType::Sprite;
	}

	bool MatchesEmitterType(FParticleEmitterInstance* Instance, EDynamicEmitterType Type)
	{
		switch (Type)
		{
		case EDynamicEmitterType::Beam: return dynamic_cast<FParticleBeamEmitterInstance*>(Instance) != nullptr;
		case EDynamicEmitterType::Ribbon: return dynamic_cast<FParticleRibbonEmitterInstance*>(Instance) != nullptr;
		case EDynamicEmitterType::Mesh: return dynamic_cast<FParticleMeshEmitterInstance*>(Instance) != nullptr;
		case EDynamicEmitterType::Sprite: return dynamic_cast<FParticleSpriteEmitterInstance*>(Instance) != nullptr;
		default: return false;
		}
	}
}

UParticleEmitter* UParticleSystemComponent::GetBeamEmitterTemplate(int32 EmitterIndex) const
{
	if (!Template || EmitterIndex < 0 || EmitterIndex >= Template->Emitters.Num())
	{
		return nullptr;
	}
	UParticleEmitter* Emitter = Template->Emitters[EmitterIndex];
	return Emitter && GetTemplateEmitterType(Emitter) == EDynamicEmitterType::Beam ? Emitter : nullptr;
}

bool UParticleSystemComponent::SetBeamSourcePoint(int32 EmitterIndex, const FVector& Position)
{
	if (!std::isfinite(Position.X) || !std::isfinite(Position.Y) || !std::isfinite(Position.Z)) return false;
	auto* Instance = GetBeamInstance(EmitterIndex);
	return Instance && Instance->SetSourcePoint(Position);
}

bool UParticleSystemComponent::SetBeamTargetPoint(int32 EmitterIndex, const FVector& Position)
{
	if (!std::isfinite(Position.X) || !std::isfinite(Position.Y) || !std::isfinite(Position.Z)) return false;
	auto* Instance = GetBeamInstance(EmitterIndex);
	return Instance && Instance->SetTargetPoint(Position);
}

bool UParticleSystemComponent::ClearBeamSourcePoint(int32 EmitterIndex)
{
	auto* Instance = GetBeamInstance(EmitterIndex);
	if (!Instance) return false;
	Instance->ClearSourcePoint();
	return true;
}

bool UParticleSystemComponent::ClearBeamTargetPoint(int32 EmitterIndex)
{
	auto* Instance = GetBeamInstance(EmitterIndex);
	if (!Instance) return false;
	Instance->ClearTargetPoint();
	return true;
}

FParticleBeamEmitterInstance* UParticleSystemComponent::GetBeamInstance(int32 EmitterIndex)
{
	UParticleEmitter* Emitter = GetBeamEmitterTemplate(EmitterIndex);
	if (!Emitter) return nullptr;
	EnsureEmitterInstances();
	for (auto* Instance : EmitterInstances)
	{
		if (Instance && Instance->SpriteTemplate == Emitter)
			return dynamic_cast<FParticleBeamEmitterInstance*>(Instance);
	}
	return nullptr;
}

void UParticleSystemComponent::EnsureEmitterInstances()
{
	if (!Template) return;
	int32 ExpectedCount = 0;
	for (auto* Emitter : Template->Emitters)
	{
		if (!Emitter) continue;
		++ExpectedCount;
		bool bFound = false;
		for (auto* Instance : EmitterInstances)
			if (Instance && Instance->SpriteTemplate == Emitter && MatchesEmitterType(Instance, GetTemplateEmitterType(Emitter)))
			{
				bFound = true;
				break;
			}
		if (!bFound) { InitParticles(); return; }
	}
	if (ExpectedCount != EmitterInstances.Num()) InitParticles();
}

bool UParticleSystemComponent::SetBeamActorParameter(const FString& Name, AActor* Actor)
{
	const FName ParameterName(Name);
	if (Name.empty() || ParameterName.IsNone() || !FParticleBeamEmitterInstance::IsRegisteredObject(Actor)) return false;
	EnsureEmitterInstances();
	bool bDelivered = false;
	for (auto* Instance : EmitterInstances)
		if (auto* Beam = dynamic_cast<FParticleBeamEmitterInstance*>(Instance))
			bDelivered |= Beam->SetActorParameter(ParameterName, Actor);
	return bDelivered;
}

bool UParticleSystemComponent::SetBeamComponentParameter(const FString& Name, USceneComponent* SceneComponent)
{
	const FName ParameterName(Name);
	if (Name.empty() || ParameterName.IsNone() || !FParticleBeamEmitterInstance::IsRegisteredObject(SceneComponent)) return false;
	EnsureEmitterInstances();
	bool bDelivered = false;
	for (auto* Instance : EmitterInstances)
		if (auto* Beam = dynamic_cast<FParticleBeamEmitterInstance*>(Instance))
			bDelivered |= Beam->SetComponentParameter(ParameterName, SceneComponent);
	return bDelivered;
}

bool UParticleSystemComponent::ClearBeamParameter(const FString& Name)
{
	const FName ParameterName(Name);
	if (Name.empty() || ParameterName.IsNone()) return false;
	EnsureEmitterInstances();
	bool bDelivered = false;
	for (auto* Instance : EmitterInstances)
		if (auto* Beam = dynamic_cast<FParticleBeamEmitterInstance*>(Instance))
		{
			Beam->ClearParameter(ParameterName);
			bDelivered = true;
		}
	return bDelivered;
}

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

	if (Template && GetCurrentLODMethod() == EParticleSystemLODMethod::Automatic)
	{
		AccumLODDistanceCheckTime += DeltaTime;
		if (AccumLODDistanceCheckTime > Template->LODDistanceCheckTime)
		{
			AccumLODDistanceCheckTime = 0.0f;
			SetLODLevel(DetermineLODLevelForLocation(GetWorldLocation()));
		}
	}

	DeltaTime *= CustomTimeDilation;

	if (IsActive() && Template && EmitterInstances.IsEmpty())
	{
		InitParticles();
	}

	for (auto* EmitterInstance : EmitterInstances)
	{
		EmitterInstance->Tick(DeltaTime);
	}

	FPrimitiveSceneProxy* Proxy = GetSceneProxy();
	FBox NewBox{};
	const bool bUseFixedBounds = Template && Template->bUseFixedRelativeBoundingBox;
	if (!bUseFixedBounds && Proxy && GetEmitterBoundingBox(NewBox))
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
	if (Template && Template->bUseFixedRelativeBoundingBox)
	{
		return Template->FixedRelativeBoundingBox.GetWorldAABB(GetWorldMatrix());
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
	UStaticMesh* Mesh = UAssetManager::GetAssetByPath<UStaticMesh>("Cube");

	if (!Mesh)
	{
		assert(false);
		return;
	}

	auto* Emitter = new FParticleMeshEmitterInstance();

	Emitter->Component = this;

	UMaterial* BaseMaterial = Mesh->GetMaterial(0);

	if (BaseMaterial)
	{
		UMaterial* TestMaterial =
		    UMaterial::CreateInstance(BaseMaterial);

		TestMaterial->SetBlendState(EBlendState::Opaque);

		// RGB는 기존 색 유지, alpha만 반투명으로 설정.
		// TestMaterial->BaseColor.W = 0.35f;

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

	SetReceiveShadow(true);
	SetCastShadow(true);
	SetReceiveDecal(true);

	OnTransformDirty();
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

	// Reuse matching beam instances so their external inputs survive a restart.
	TArray<FParticleEmitterInstance*> PreviousInstances = std::move(EmitterInstances);
	EmitterInstances.Empty();
	LODLevel = ClampLODLevel(LODLevel);

	for (auto* Emitter : Template->Emitters)
	{
		if (!Emitter)
		{
			continue;
		}
		Emitter->CacheEmitterModuleInfo();

		FParticleEmitterInstance* ParticleEmitterInstance = nullptr;

		switch (Emitter->EmitterType)
		{
		case EDynamicEmitterType::Sprite:
			ParticleEmitterInstance =
			    new FParticleSpriteEmitterInstance();
			break;

		case EDynamicEmitterType::Mesh:
			ParticleEmitterInstance =
			    new FParticleMeshEmitterInstance();
			break;

		case EDynamicEmitterType::Beam:
			for (auto*& Previous : PreviousInstances)
			{
				if (Previous && Previous->SpriteTemplate == Emitter &&
				    dynamic_cast<FParticleBeamEmitterInstance*>(Previous))
				{
					ParticleEmitterInstance = Previous;
					Previous = nullptr;
					break;
				}
			}
			if (!ParticleEmitterInstance) ParticleEmitterInstance = new FParticleBeamEmitterInstance();
			break;

		case EDynamicEmitterType::Ribbon:
			ParticleEmitterInstance =
			    new FParticleRibbonEmitterInstance();
			break;

		default:
			continue;
		}

		ParticleEmitterInstance->InitParameters(Emitter, this);
		ParticleEmitterInstance->SetCurrentLODIndex(LODLevel);
		ParticleEmitterInstance->bHaltSpawning |= !IsActive() || bSystemDeactivated;
		EmitterInstances.Add(ParticleEmitterInstance);
	}
	for (auto* Previous : PreviousInstances) delete Previous;
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

void UParticleSystemComponent::PostApplyProperties(const FProperty* ChangedProperty)
{
	Super::PostApplyProperties(ChangedProperty);

	if (ChangedProperty && ChangedProperty->Name == FName("Template"))
	{
		ResetParticles();
		if (IsActive() && Template)
		{
			InitParticles();
		}
		OnTransformDirty();
	}
}

void UParticleSystemComponent::SetActive(bool bNewActive)
{
	Super::SetActive(bNewActive);

	if (bNewActive)
	{
		ActivateSystem();
	}
	else
	{
		DeactivateSystem();
	}
}

void UParticleSystemComponent::ActivateSystem(bool bReset)
{
	bSystemDeactivated = false;
	if (!Template)
	{
		return;
	}

	if (GetCurrentLODMethod() == EParticleSystemLODMethod::ActivateAutomatic)
	{
		SetLODLevel(DetermineLODLevelForLocation(GetWorldLocation()));
	}

	if (bReset || EmitterInstances.IsEmpty())
	{
		InitParticles();
		return;
	}

	for (FParticleEmitterInstance* EmitterInstance : EmitterInstances)
	{
		EmitterInstance->bHaltSpawning = false;
	}
}

void UParticleSystemComponent::DeactivateSystem()
{
	bSystemDeactivated = true;
	for (FParticleEmitterInstance* EmitterInstance : EmitterInstances)
	{
		EmitterInstance->bHaltSpawning = true;
	}
}

void UParticleSystemComponent::ResetSystem()
{
	bSystemDeactivated = false;
	for (FParticleEmitterInstance* EmitterInstance : EmitterInstances)
	{
		EmitterInstance->KillParticlesForced();
		EmitterInstance->Rewind();
		EmitterInstance->bHaltSpawning = false;
	}
}

bool UParticleSystemComponent::HasCompleted() const
{
	if (EmitterInstances.IsEmpty())
	{
		return false;
	}

	for (const FParticleEmitterInstance* EmitterInstance : EmitterInstances)
	{
		if (!EmitterInstance->HasCompleted())
		{
			return false;
		}
	}
	return true;
}

void UParticleSystemComponent::SetLODLevel(int32 InLODLevel)
{
	InLODLevel = ClampLODLevel(InLODLevel);
	if (LODLevel == InLODLevel)
	{
		return;
	}

	LODLevel = InLODLevel;
	for (FParticleEmitterInstance* EmitterInstance : EmitterInstances)
	{
		EmitterInstance->SetCurrentLODIndex(LODLevel);
	}
}

int32 UParticleSystemComponent::DetermineLODLevelForLocation(const FVector& EffectLocation)
{
	if (!Template || Template->GetLODLevelCount() <= 1 || GetCurrentLODMethod() == EParticleSystemLODMethod::DirectSet)
	{
		return 0;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return LODLevel;
	}

	TArray<FVector> ViewLocations;
	APlayerCameraManager* CameraManager = World->GetPlayerCameraManager();
	if (CameraManager && CameraManager->HasValidCameraView())
	{
		ViewLocations.Add(CameraManager->GetCameraView().GetLocation());
	}
	else if (const TArray<FVector>* RenderedLocations = World->GetViewLocationsRenderedLastFrame())
	{
		ViewLocations = *RenderedLocations;
	}

	if (ViewLocations.IsEmpty())
	{
		return LODLevel;
	}

	float ClosestDistanceSquared = FLT_MAX;
	for (const FVector& ViewLocation : ViewLocations)
	{
		ClosestDistanceSquared = std::min(ClosestDistanceSquared, (ViewLocation - EffectLocation).SizeSquared());
	}

	int32 Result = 0;
	for (int32 LODIndex = 1; LODIndex < Template->GetLODLevelCount(); ++LODIndex)
	{
		const float LODDistance = Template->LODDistances[LODIndex];
		if (ClosestDistanceSquared < LODDistance * LODDistance)
		{
			break;
		}
		Result = LODIndex;
	}
	return Result;
}

EParticleSystemLODMethod UParticleSystemComponent::GetCurrentLODMethod() const
{
	if (bOverrideLODMethod)
	{
		return LODMethod;
	}
	return Template ? Template->LODMethod : EParticleSystemLODMethod::DirectSet;
}

int32 UParticleSystemComponent::ClampLODLevel(int32 InLODLevel) const
{
	const int32 LODCount = Template ? Template->GetLODLevelCount() : 0;
	if (LODCount <= 1)
	{
		return 0;
	}
	return std::clamp(InLODLevel, 0, LODCount - 1);
}

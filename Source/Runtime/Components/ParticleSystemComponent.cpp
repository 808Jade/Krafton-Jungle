#include "EnginePCH.h"
#include "ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Components/PointLightComponent.h"
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
	// Animated emitters may be repositioned by PostPhysics animation notifies.
	PrimaryComponentTick.TickGroup = ETickingGroup::PostUpdateWork;
}

UParticleSystemComponent::~UParticleSystemComponent()
{
	ResetParticles();
	DestroyParticleLights();
}

UPointLightComponent* UParticleSystemComponent::AcquireParticleLight()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	if (ParticleLightsUsed >= ParticleLights.Num())
	{
		if (ParticleLights.Num() >= MaxParticleLights)
		{
			return nullptr;
		}

		// 액터의 컴포넌트 목록에 넣지 않고 월드에만 등록한다. 씬 저장에 들어가지 않는다.
		UPointLightComponent* NewLight = NewObject<UPointLightComponent>(this);
		NewLight->SetCastShadows(false);
		NewLight->RegisterComponentWithWorld(World);
		ParticleLights.Add(NewLight);
	}

	UPointLightComponent* Light = ParticleLights[ParticleLightsUsed++];
	Light->SetVisibility(true);
	return Light;
}

void UParticleSystemComponent::BeginParticleLights()
{
	ParticleLightsUsed = 0;
}

void UParticleSystemComponent::EndParticleLights()
{
	// 이번 프레임에 빌려 가지 않은 라이트는 끈다. (칸은 유지하고 영향만 없앤다)
	for (int32 i = ParticleLightsUsed; i < ParticleLights.Num(); ++i)
	{
		ParticleLights[i]->SetVisibility(false);
	}
}

void UParticleSystemComponent::DestroyParticleLights()
{
	for (UPointLightComponent* Light : ParticleLights)
	{
		if (!Light)
		{
			continue;
		}
		if (Light->IsRegistered())
		{
			Light->UnregisterComponent();
		}
		delete Light;
	}
	ParticleLights.Empty();
	ParticleLightsUsed = 0;
}

void UParticleSystemComponent::OnUnregister()
{
	// 월드가 아직 살아 있을 때 라이트를 씬에서 뺀다. (소멸자에서는 월드가 이미 없을 수 있다)
	DestroyParticleLights();
	Super::OnUnregister();
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
		ActivateSystem();
	}

	if (bPendingActivateLOD)
	{
		int32 DesiredLODLevel = LODLevel;
		if (GetCurrentLODMethod() != EParticleSystemLODMethod::ActivateAutomatic)
		{
			bPendingActivateLOD = false;
		}
		else if (TryDetermineLODLevelForLocation(GetWorldLocation(), DesiredLODLevel))
		{
			bPendingActivateLOD = false;
			SetLODLevel(DesiredLODLevel);
		}
	}

	BeginParticleLights();
	for (auto* EmitterInstance : EmitterInstances)
	{
		EmitterInstance->Tick(DeltaTime);
	}
	EndParticleLights();

	// 이번 프레임에 모인 충돌 이벤트는 모든 이미터 갱신이 끝난 뒤에 전달한다.
	FlushCollisionEvents();

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

void UParticleSystemComponent::QueueCollisionEvent(FParticleCollisionEvent&& Event)
{
	// 개수 제한은 이벤트를 만드는 Collision 모듈의 MaxEventsPerFrame이 맡는다.
	PendingCollisionEvents.Add(std::move(Event));
}

void UParticleSystemComponent::FlushCollisionEvents()
{
	// 이번 프레임에 이미터가 읽은 이벤트는 여기서 버린다. (이벤트는 만들어진 다음 프레임 한 번만 읽힌다)
	ReceivedCollisionEvents.Reset();
	if (PendingCollisionEvents.IsEmpty())
	{
		return;
	}

	// 콜백 안에서 새 이벤트가 쌓여도 이번 순회에 섞이지 않게, 목록을 먼저 꺼내 둔다.
	// 같은 목록을 다음 프레임의 Event Receiver 모듈이 읽도록 보관한다.
	ReceivedCollisionEvents = std::move(PendingCollisionEvents);
	PendingCollisionEvents.Reset();
	for (int32 Index = 0; Index < ReceivedCollisionEvents.Num(); ++Index)
	{
		OnParticleCollide.Broadcast(ReceivedCollisionEvents[Index]);
	}
}

void UParticleSystemComponent::RefreshEventReceivers()
{
	bHasEventReceivers = false;
	if (!Template)
	{
		return;
	}

	for (UParticleEmitter* Emitter : Template->Emitters)
	{
		if (!Emitter)
		{
			continue;
		}
		for (UParticleLODLevel* LODLevelData : Emitter->LODLevels)
		{
			if (!LODLevelData)
			{
				continue;
			}
			for (UParticleModule* Module : LODLevelData->Modules)
			{
				if (Module && Module->bEnabled && Module->ReceivesParticleEvents())
				{
					bHasEventReceivers = true;
					return;
				}
			}
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
	// 템플릿에 Event Receiver 모듈이 있는지 다시 확인한다. (충돌 이벤트를 모을지 결정)
	RefreshEventReceivers();
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
			ActivateSystem(true);
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

	if (GetCurrentLODMethod() != EParticleSystemLODMethod::DirectSet)
	{
		int32 DesiredLODLevel = LODLevel;
		bPendingActivateLOD = !TryDetermineLODLevelForLocation(GetWorldLocation(), DesiredLODLevel);
		SetLODLevel(DesiredLODLevel);
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
	int32 Result = LODLevel;
	TryDetermineLODLevelForLocation(EffectLocation, Result);
	return Result;
}

bool UParticleSystemComponent::TryDetermineLODLevelForLocation(const FVector& EffectLocation, int32& OutLODLevel) const
{
	if (!Template || Template->GetLODLevelCount() <= 1 || GetCurrentLODMethod() == EParticleSystemLODMethod::DirectSet)
	{
		OutLODLevel = 0;
		return true;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
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
		return false;
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
	OutLODLevel = Result;
	return true;
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

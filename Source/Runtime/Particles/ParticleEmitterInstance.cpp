#include "EnginePCH.h"

#include "Core/HAL/UnrealMemory.h"

#include "ParticleEmitterInstance.h"
#include "ParticleHelper.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/Modules/ParticleModuleRequired.h"
#include "Particles/Modules/ParticleModuleSpawn.h"
#include "Particles/Modules/ParticleModuleTypeDataMesh.h"

#include "Components/ParticleSystemComponent.h"

#include "Asset/AssetManager.h"

#include "Math/Transform.h"


FParticleEmitterInstance::~FParticleEmitterInstance()
{
	Release();
}

void FParticleEmitterInstance::Tick(float DeltaTime)
{
	// 컴포넌트 트랜스폼으로 EmitterToSimulation, SimulationToWorld 갱신
	UpdateTransforms();
	// EmitterTime을 진행시키고 루프 수를 세서, 정해진 루프를 다 돌면 스폰 중단
	TickEmitterTime(DeltaTime);
	// RelativeTime을 진행시키고 수명(1.0)이 다한 파티클 제거
	AgeAndKillParticles(DeltaTime);
	// Velocity, Color, Size, RotationRate를 Base 값으로 되돌림
	ResetParticleParameters();
	// 모듈들 Update호출
	UpdateModules(DeltaTime);
	// 이번 틱에 만들 개수를 계산해 새 파티클 생성
	Spawn(DeltaTime);
	// 속도와 회전 속도에 DeltaTime을 곱해 Location, Rotation을 이동
	IntegrateParticles(DeltaTime);
	// 활성 파티클을 감싸는 ParticleBoundingBox 갱신
	UpdateBoundingBox();
}

void FParticleEmitterInstance::TickEmitterTime(float DeltaTime)
{
	PrevEmitterTime = EmitterTime;
	EmitterTime += DeltaTime;
	if (EmitterDuration > 0)
	{
		while (EmitterTime >= EmitterDuration)
		{
			EmitterTime -= EmitterDuration;
			LoopCount++;
		}
	}

	if (EmitterLoops > 0 && LoopCount >= EmitterLoops)
	{
		bEmitterIsDone = true;
	}
}

void FParticleEmitterInstance::AgeAndKillParticles(float DeltaTime)
{
	for (int32 Index = ActiveParticles - 1; Index >= 0; Index--)
	{
		uint16 NextFreeIndex = ParticleIndices[Index];
		DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * NextFreeIndex);

		Particle->RelativeTime += DeltaTime * Particle->OneOverMaxLifetime;
		if (Particle->RelativeTime >= 1)
		{
			KillParticle(Index);
		}
	}
}

void FParticleEmitterInstance::ResetParticleParameters()
{
	for (int32 Index = 0; Index < ActiveParticles; Index++)
	{
		uint16 NextFreeIndex = ParticleIndices[Index];
		DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * NextFreeIndex);

		Particle->Velocity = Particle->BaseVelocity;
		Particle->Color = Particle->BaseColor;
		Particle->Size = Particle->BaseSize;
		Particle->RotationRate = Particle->BaseRotationRate;
	}
}

void FParticleEmitterInstance::IntegrateParticles(float DeltaTime)
{
	for (int32 Index = 0; Index < ActiveParticles; Index++)
	{
		uint16 NextFreeIndex = ParticleIndices[Index];
		DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * NextFreeIndex);

		if (Particle->Flags & STATE_Particle_JustSpawned)
		{
			Particle->Flags &= ~STATE_Particle_JustSpawned;
		}
		else
		{
			Particle->OldLocation = Particle->Location;
			Particle->Location += Particle->Velocity * DeltaTime;
			Particle->Rotation += Particle->RotationRate * DeltaTime;
		}
	}
}

void FParticleEmitterInstance::UpdateBoundingBox()
{
	if (!Component || Component->bUseFixedRelativeBoundingBox || ActiveParticles <= 0)
	{
		return;
	}

	const FVector Scale = Component->GetWorldScale3D();
	const FMatrix ComponentToWorld = Component->GetWorldMatrix();

	FVector MinVal(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector MaxVal(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (int32 Index = 0; Index < ActiveParticles; Index++)
	{
		DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndices[Index]);

		const FVector Extent = (Particle->Size * Scale).GetAbs();
		const float LocalMax = std::max({ Extent.X, Extent.Y, Extent.Z });
		const FVector Position = bUseLocalSpace
		                             ? ComponentToWorld.TransformPosition(Particle->Location)
		                             : Particle->Location;

		MinVal.X = std::min(MinVal.X, Position.X - LocalMax);
		MinVal.Y = std::min(MinVal.Y, Position.Y - LocalMax);
		MinVal.Z = std::min(MinVal.Z, Position.Z - LocalMax);
		MaxVal.X = std::max(MaxVal.X, Position.X + LocalMax);
		MaxVal.Y = std::max(MaxVal.Y, Position.Y + LocalMax);
		MaxVal.Z = std::max(MaxVal.Z, Position.Z + LocalMax);
	}

	ParticleBoundingBox = FBox{ MinVal, MaxVal };
}

void FParticleEmitterInstance::UpdateTransforms()
{
	if (!Component)
	{
		return;
	}

	OldLocation = Location;
	Location = Component->GetWorldLocation();

	const FMatrix ComponentToWorld =
	    FTransform(Component->GetWorldRotation(), Component->GetWorldLocation()).GetLocalMatrix();

	// TODO: RequiredModule에 EmitterOrigin/EmitterRotation이 생기면 EmitterToComponent를 앞에 곱한다.
	if (bUseLocalSpace)
	{
		EmitterToSimulation = FMatrix::Identity;
		SimulationToWorld = ComponentToWorld;
	}
	else
	{
		EmitterToSimulation = ComponentToWorld;
		SimulationToWorld = FMatrix::Identity;
	}
}


void FParticleEmitterInstance::Spawn(float DeltaTime)
{
	const UParticleModuleSpawn* SpawnModule = SpriteTemplate ? SpriteTemplate->SpawnModule : nullptr;
	if (bEmitterIsDone || bHaltSpawning || !SpawnModule || !SpawnModule->bEnabled)
	{
		return;
	}

	const FVector4 Origin = EmitterToSimulation.GetOrigin();
	const FVector SpawnLocation(Origin.X, Origin.Y, Origin.Z);
	const FVector SpawnVelocity(0.0f, 0.0f, 0.0f);

	const FParticleSpawnBatch RateBatch = SpawnModule->GetRateBatch(DeltaTime, SpawnFraction);
	SpawnParticles(RateBatch.Count, RateBatch.StartTime, RateBatch.Increment, SpawnLocation, SpawnVelocity);

	TArray<FParticleSpawnBatch> BurstBatches;
	SpawnModule->GetBurstBatches(PrevEmitterTime, EmitterTime, EmitterDuration, BurstBatches);
	for (const FParticleSpawnBatch& Batch : BurstBatches)
	{
		SpawnParticles(Batch.Count, Batch.StartTime, Batch.Increment, SpawnLocation, SpawnVelocity);
	}
}

void FParticleEmitterInstance::UpdateModules(float DeltaTime)
{
	for (UParticleModule* UpdateModule : SpriteTemplate->UpdateModules)
	{
		if (!UpdateModule || !UpdateModule->bEnabled)
		{
			continue;
		}
		const int32* Offset = SpriteTemplate->ModuleOffsetMap.Find(UpdateModule);
		UpdateModule->Update(*this, Offset ? *Offset : 0, DeltaTime);
	}
}

void FParticleEmitterInstance::PreSpawn(FBaseParticle* Particle,
                                        const FVector& InitialLocation, const FVector& InitialVelocity)
{
	FMemory::Memzero(Particle, ParticleSize);
	Particle->Location = InitialLocation;
	Particle->OldLocation = InitialLocation;
	Particle->BaseVelocity = InitialVelocity;
	Particle->Velocity = InitialVelocity;
}

void FParticleEmitterInstance::PostSpawn(FBaseParticle* Particle,
                                         float InterpolationPercentage, float SpawnTime)
{
	if (!bUseLocalSpace)
	{
		if (FVector::Distance(OldLocation, Location) > 0.01f)
		{
			Particle->Location += (OldLocation - Location) * InterpolationPercentage;
		}
	}

	Particle->OldLocation = Particle->Location;
	Particle->Location += FVector(Particle->Velocity) * SpawnTime;

	Particle->Flags |= ((ParticleCounter++) & STATE_CounterMask);
	Particle->Flags |= STATE_Particle_JustSpawned;
}

void FParticleEmitterInstance::SpawnParticles(int32 Count, float StartTime, float Increment,
                                              const FVector& InitialLocation, const FVector& InitialVelocity)
{
	float SpawnTime;
	float InterpolationPercentage = 1.0f;

	for (int32 Index = 0; Index < Count; Index++)
	{
		if (ActiveParticles >= MaxActiveParticles)
		{
			return;
		}
		uint16 NextFreeIndex = ParticleIndices[ActiveParticles];
		DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * NextFreeIndex);
		SpawnTime = StartTime - Increment * Index;
		PreSpawn(Particle, InitialLocation, InitialVelocity);

		for (UParticleModule* SpawnModule : SpriteTemplate->SpawnModules)
		{
			if (!SpawnModule || !SpawnModule->bEnabled)
			{
				continue;
			}
			const int32* Offset = SpriteTemplate->ModuleOffsetMap.Find(SpawnModule);
			SpawnModule->Spawn(*this, Offset ? *Offset : 0, SpawnTime, *Particle);
		}
		
		PostSpawn(Particle, InterpolationPercentage, SpawnTime);
		InterpolationPercentage = InterpolationPercentage != 0 ? InterpolationPercentage - 1.0f / Count : 0;
		ActiveParticles++;
	}
}

void FParticleEmitterInstance::KillParticle(int32 Index)
{
	if (Index < 0 || Index >= ActiveParticles)
	{
		return;
	}
	std::swap(ParticleIndices[Index], ParticleIndices[ActiveParticles - 1]);
	ActiveParticles--;
}

void FParticleEmitterInstance::KillParticlesForced()
{
	ActiveParticles = 0;
}

void FParticleEmitterInstance::Rewind()
{
	EmitterTime = 0.0f;
	PrevEmitterTime = 0.0f;
	LoopCount = 0;
	SpawnFraction = 0.0f;
	bEmitterIsDone = false;
}

bool FParticleEmitterInstance::HasCompleted() const
{
	return (bEmitterIsDone || bHaltSpawning) && ActiveParticles == 0;
}

void FParticleEmitterInstance::Init(int32 InMaxParticles, int32 InParticleSize)
{
	if (ParticleData)
	{
		Release();
	}

	MaxActiveParticles = std::clamp(InMaxParticles, 1, 65535);
	ParticleSize = InParticleSize;
	ParticleStride = static_cast<int32>(FMemory::QuantizeSize(InParticleSize, 16));
	int32 BlockSize = MaxActiveParticles * ParticleStride +
	                  MaxActiveParticles * sizeof(uint16);
	ParticleData = (uint8*)FMemory::Malloc(BlockSize);
	if (!ParticleData)
	{
		ParticleSize = 0;
		ParticleStride = 0;
		MaxActiveParticles = 0;
		return;
	}
	ParticleIndices = (uint16*)(ParticleData + MaxActiveParticles * ParticleStride);
	for (uint16 Index = 0; Index < MaxActiveParticles; Index++)
	{
		ParticleIndices[Index] = Index;
	}
	ActiveParticles = 0;
	ParticleCounter = 0;
	SpawnFraction = 0.0f;
}

void FParticleEmitterInstance::Release()
{
	if (ParticleData)
	{
		FMemory::Free(ParticleData);
		ParticleData = nullptr;
		ParticleIndices = nullptr;
		ActiveParticles = 0;
		MaxActiveParticles = 0;
		ParticleSize = 0;
		ParticleStride = 0;
	}
}

bool FParticleEmitterInstance::FillReplayData(
    FDynamicEmitterReplayDataBase& OutDynamicEmitterReplayDataBase) const
{
	if (!ParticleData || ActiveParticles <= 0 || !Component)
	{
		return false;
	}
	OutDynamicEmitterReplayDataBase.ActiveParticleCount = ActiveParticles;
	OutDynamicEmitterReplayDataBase.ParticleStride = ParticleStride;
	OutDynamicEmitterReplayDataBase.Scale = Component->GetWorldScale3D();

	OutDynamicEmitterReplayDataBase.DataContainer.CopyFrom(
	    ParticleData,
	    MaxActiveParticles * ParticleStride,
	    ParticleIndices,
	    ActiveParticles);

	FDynamicSpriteEmitterReplayDataBase* SpriteReplayDataBase =
	    static_cast<FDynamicSpriteEmitterReplayDataBase*>(&OutDynamicEmitterReplayDataBase);
	SpriteReplayDataBase->bUseLocalSpace = bUseLocalSpace;

	const UParticleModuleRequired* Required = CurrentLODLevel ? CurrentLODLevel->RequiredModule : nullptr;
	if (Required)
	{
		OutDynamicEmitterReplayDataBase.SortMode = Required->SortMode;
		SpriteReplayDataBase->ScreenAlignment = Required->ScreenAlignment;
		SpriteReplayDataBase->AxisLock = Required->LockAxis;
	}

	return true;
}

UMaterialInterface* FParticleEmitterInstance::GetCurrentMaterial() const
{
	if (CurrentMaterial)
	{
		return CurrentMaterial;
	}
	return UAssetManager::GetAssetByPath<UMaterial>("DefaultParticleMaterial");
}

FDynamicEmitterDataBase* FParticleEmitterInstance::GetDynamicData() const
{
	return nullptr;
}

void FParticleEmitterInstance::InitParameters(
	UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent)
{
	SpriteTemplate = InTemplate;
	Component = InComponent;
	CurrentLODLevelIndex = 0;
	CurrentLODLevel = InTemplate->LODLevels.IsEmpty() ? nullptr : InTemplate->LODLevels[0];
	UParticleModuleRequired* Required = CurrentLODLevel ? CurrentLODLevel->RequiredModule : nullptr;
	Rewind();
	bHaltSpawning = false;

	Location = Component ? Component->GetWorldLocation() : FVector(0.0f, 0.0f, 0.0f);
	OldLocation = Location;

	if (Required)
	{
		bUseLocalSpace = Required->bUseLocalSpace;
		CurrentMaterial = Required->Material;
		EmitterDuration = Required->EmitterDuration;
		EmitterLoops = Required->EmitterLoops;
	}
	Init(InTemplate->MaxActiveParticles, InTemplate->ParticleSize);
}

/*-----------------------------------------------------------------------------
    FParticleSpriteEmitterInstance
-----------------------------------------------------------------------------*/

bool FParticleSpriteEmitterInstance::FillReplayData(
    FDynamicEmitterReplayDataBase& OutDynamicEmitterReplayDataBase) const
{
	if (!FParticleEmitterInstance::FillReplayData(OutDynamicEmitterReplayDataBase))
	{
		return false;
	}

	OutDynamicEmitterReplayDataBase.eEmitterType = EDynamicEmitterType::Sprite;

	FDynamicSpriteEmitterReplayData* SpriteReplayData =
	    static_cast<FDynamicSpriteEmitterReplayData*>(&OutDynamicEmitterReplayDataBase);
	SpriteReplayData->MaterialInterface = GetCurrentMaterial();
	return true;
}

FDynamicEmitterDataBase* FParticleSpriteEmitterInstance::GetDynamicData() const
{
	FDynamicSpriteEmitterReplayData ReplayData;
	if (!FillReplayData(ReplayData))
	{
		return nullptr;
	}

	return new FDynamicSpriteEmitterData(std::move(ReplayData));
}

/*-----------------------------------------------------------------------------
    FParticleMeshEmitterInstance
-----------------------------------------------------------------------------*/


bool FParticleMeshEmitterInstance::FillReplayData(
    FDynamicEmitterReplayDataBase& OutDynamicEmitterReplayDataBase) const
{
	if (!TestMesh)
		return false;

	if (!FParticleEmitterInstance::FillReplayData(OutDynamicEmitterReplayDataBase))
	{
		return false;
	}

	OutDynamicEmitterReplayDataBase.eEmitterType = EDynamicEmitterType::Mesh;

	FDynamicMeshEmitterReplayData* MeshReplayData =
		static_cast<FDynamicMeshEmitterReplayData*>(&OutDynamicEmitterReplayDataBase);
	MeshReplayData->Mesh = MeshTypeData ? MeshTypeData->Mesh : nullptr;
	MeshReplayData->MeshRotationPayloadOffset = MeshRotationOffset;
	MeshReplayData->MaterialInterface = nullptr;

	return true;
}

void FParticleMeshEmitterInstance::InitParameters(
	UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent)
{
	FParticleEmitterInstance::InitParameters(InTemplate, InComponent);

	MeshTypeData = CurrentLODLevel ? Cast<UParticleModuleTypeDataMesh>(CurrentLODLevel->TypeDataModule) : nullptr;

	MeshRotationOffset = InvalidPayloadOffset;
	if (MeshTypeData)
	{
		if (const int32* Offset = InTemplate->ModuleOffsetMap.Find(MeshTypeData))
		{
			MeshRotationOffset = *Offset;
		}
	}
}

FDynamicEmitterDataBase* FParticleMeshEmitterInstance::GetDynamicData() const
{
	FDynamicMeshEmitterReplayData ReplayData;
	if (!FillReplayData(ReplayData))
	{
		return nullptr;
	}

	return new FDynamicMeshEmitterData(std::move(ReplayData));
}

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
#include "Particles/Modules/ParticleModuleTypeDataBeam.h"
#include "Particles/Modules/ParticleModuleTypeDataRibbon.h"
#include "Particles/Modules/ParticleModuleBeamSource.h"
#include "Particles/Modules/ParticleModuleBeamTarget.h"
#include "Particles/Modules/ParticleModuleBeamNoise.h"

#include "Components/ParticleSystemComponent.h"

#include "Asset/AssetManager.h"

#include "Math/Transform.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cfloat>


namespace
{
	FVector GetTimedNoiseOffset(
	    const FBeamNoisePayload& Noise,
	    int32 Index,
	    float UpdateInterval)
	{
		if (UpdateInterval <= 0.0f)
		{
			return Noise.FromOffsets[Index];
		}

		const float Interval =
		    std::max(UpdateInterval, 0.01f);

		float Alpha = std::clamp(
		    Noise.Elapsed / Interval,
		    0.0f,
		    1.0f);

		// Smoothstep: 시간 구간 경계에서 변화 속도를 부드럽게.
		Alpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);

		return Noise.FromOffsets[Index] * (1.0f - Alpha) +
		       Noise.ToOffsets[Index] * Alpha;
	}

	FVector SampleBeamNoise(
	    const FBeamNoisePayload& Noise,
	    float T,
	    float UpdateInterval,
		bool bSmoothNoise)
	{
		const int32 Count = Noise.SegmentCount;

		const float X =
		    std::clamp(T, 0.0f, 1.0f) *
		    static_cast<float>(Count);

		const int32 Index = std::min(
		    static_cast<int32>(X),
		    Count - 1);

		const float U = X - static_cast<float>(Index);

		const FVector P1 =
		    GetTimedNoiseOffset(Noise, Index, UpdateInterval);

		const FVector P2 =
		    GetTimedNoiseOffset(Noise, Index + 1, UpdateInterval);

		if (!bSmoothNoise)
		{
			return P1 * (1.0f - U) + P2 * U;
		}

		const FVector P0 = GetTimedNoiseOffset(
		    Noise,
		    std::max(Index - 1, 0),
		    UpdateInterval);

		const FVector P3 = GetTimedNoiseOffset(
		    Noise,
		    std::min(Index + 2, Count),
		    UpdateInterval);

		// Catmull-Rom 보간
		const float U2 = U * U;
		const float U3 = U2 * U;

		// Uniform Catmull–Rom의 계수.
		const float W0 = -0.5f * U + U2 - 0.5f * U3;
		const float W1 = 1.0f - 2.5f * U2 + 1.5f * U3;
		const float W2 = 0.5f * U + 2.0f * U2 - 1.5f * U3;
		const float W3 = -0.5f * U2 + 0.5f * U3;

		return P0 * W0 + P1 * W1 + P2 * W2 + P3 * W3;
	}
}


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

void FParticleEmitterInstance::SetCurrentLODIndex(int32 LODIndex)
{
	if (!SpriteTemplate || SpriteTemplate->LODLevels.IsEmpty())
	{
		return;
	}
	LODIndex = std::clamp(LODIndex, 0, SpriteTemplate->LODLevels.Num() - 1);

	const UParticleLODLevel* RequestedLODLevel = SpriteTemplate->LODLevels[LODIndex];
	if (!RequestedLODLevel || !RequestedLODLevel->bModuleLayoutValid)
	{
		LODIndex = 0;
	}

	UParticleLODLevel* NewLODLevel = SpriteTemplate->LODLevels[LODIndex];
	if (!NewLODLevel)
	{
		return;
	}

	CurrentLODLevelIndex = LODIndex;
	CurrentLODLevel = NewLODLevel;

	UParticleModuleRequired* Required = CurrentLODLevel->RequiredModule;
	if (Required)
	{
		CurrentMaterial = Required->Material;
		EmitterDuration = Required->EmitterDuration;
		EmitterLoops = Required->EmitterLoops;
	}

	if (EmitterDuration > 0.0f && EmitterTime >= EmitterDuration)
	{
		EmitterTime = std::fmod(EmitterTime, EmitterDuration);
		PrevEmitterTime = EmitterTime;
	}

	if (!CurrentLODLevel->bEnabled)
	{
		KillParticlesForced();
	}
}

void FParticleEmitterInstance::UpdateBoundingBox()
{
	if (!Component || (Component->Template && Component->Template->bUseFixedRelativeBoundingBox) || ActiveParticles <= 0)
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
	const UParticleModuleSpawn* SpawnModule = CurrentLODLevel ? CurrentLODLevel->SpawnModule : nullptr;
	if (bEmitterIsDone || bHaltSpawning || !SpawnModule || !SpawnModule->bEnabled || !CurrentLODLevel || !CurrentLODLevel->bEnabled)
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
	if (!CurrentLODLevel)
	{
		return;
	}
	for (UParticleModule* UpdateModule : CurrentLODLevel->UpdateModules)
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

		if (CurrentLODLevel)
		{
			for (UParticleModule* SpawnModule : CurrentLODLevel->SpawnModules)
			{
				if (!SpawnModule || !SpawnModule->bEnabled)
				{
					continue;
				}
				const int32* Offset = SpriteTemplate->ModuleOffsetMap.Find(SpawnModule);
				SpawnModule->Spawn(*this, Offset ? *Offset : 0, SpawnTime, *Particle);
			}
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
	if (!ParticleData || ActiveParticles <= 0 || !Component || !CurrentLODLevel || !CurrentLODLevel->bEnabled)
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

	const UParticleModuleRequired* Required = CurrentLODLevel ? CurrentLODLevel->RequiredModule : nullptr;

	OutDynamicEmitterReplayDataBase.SortMode =
	    Required
	        ? Required->SortMode
	        : EParticleSortMode::None;

	return true;
}

void FParticleEmitterInstance::FillSpriteReplaySettings(
    FDynamicSpriteEmitterReplayDataBase& OutData) const
{
	OutData.bUseLocalSpace = bUseLocalSpace;

	const UParticleModuleRequired* Required =
	    CurrentLODLevel
	        ? CurrentLODLevel->RequiredModule
	        : nullptr;

	if (Required)
	{
		OutData.ScreenAlignment = Required->ScreenAlignment;
		OutData.AxisLock = Required->LockAxis;
	}
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
	FillSpriteReplaySettings(*SpriteReplayData);
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

	if (!FParticleEmitterInstance::FillReplayData(OutDynamicEmitterReplayDataBase))
	{
		return false;
	}

	OutDynamicEmitterReplayDataBase.eEmitterType = EDynamicEmitterType::Mesh;

	FDynamicMeshEmitterReplayData* MeshReplayData =
	    static_cast<FDynamicMeshEmitterReplayData*>(&OutDynamicEmitterReplayDataBase);
	FillSpriteReplaySettings(*MeshReplayData);
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

void FParticleMeshEmitterInstance::SetCurrentLODIndex(int32 LODIndex)
{
	FParticleEmitterInstance::SetCurrentLODIndex(LODIndex);

	MeshTypeData = CurrentLODLevel ? Cast<UParticleModuleTypeDataMesh>(CurrentLODLevel->TypeDataModule) : nullptr;
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

void FParticleBeamEmitterInstance::InitParameters(
    UParticleEmitter* InTemplate,
    UParticleSystemComponent* InComponent)
{
	FParticleEmitterInstance::InitParameters(
	    InTemplate,
	    InComponent);

	BeamTypeData = CurrentLODLevel
	                   ? Cast<UParticleModuleTypeDataBeam>(
	                         CurrentLODLevel->TypeDataModule)
	                   : nullptr;

	SourceModule = nullptr;
	TargetModule = nullptr;
	NoiseModule = nullptr;
	BeamPayloadOffset = InvalidPayloadOffset;
	NoisePayloadOffset = InvalidPayloadOffset;

	if (BeamTypeData)
	{
		if (const int32* Offset =
		        InTemplate->ModuleOffsetMap.Find(BeamTypeData))
		{
			BeamPayloadOffset = *Offset;
		}
	}

	if (CurrentLODLevel)
	{
		// 첫 번째 활성 Source/Target 모듈을 사용.
		for (UParticleModule* Module : CurrentLODLevel->Modules)
		{
			if (!Module || !Module->bEnabled)
			{
				continue;
			}

			if (!SourceModule)
			{
				SourceModule =
				    Cast<UParticleModuleBeamSource>(Module);
			}

			if (!TargetModule)
			{
				TargetModule =
				    Cast<UParticleModuleBeamTarget>(Module);
			}

			if (!NoiseModule)
			{
				NoiseModule =
				    Cast<UParticleModuleBeamNoise>(Module);
			}
		}
	}

	if (NoiseModule)
	{
		if (const int32* Offset =
		        InTemplate->ModuleOffsetMap.Find(NoiseModule))
		{
			NoisePayloadOffset = *Offset;
		}
	}

	if (!HasValidBeamSetup())
	{
		bHaltSpawning = true;

		HTR_LOG(
		    Error,
		    "Beam emitter requires enabled TypeData, Source, "
		    "Target modules and a valid payload.");
	}
}

void FParticleBeamEmitterInstance::PostSpawn(
    FBaseParticle* Particle,
    float InterpolationPercentage,
    float SpawnTime)
{
	// 생성 카운터와 JustSpawned 플래그 등 기존 공통 처리.
	FParticleEmitterInstance::PostSpawn(
	    Particle,
	    InterpolationPercentage,
	    SpawnTime);

	if (!HasValidBeamSetup())
	{
		// 잘못된 설정에서 생성된 파티클은 렌더링하지 않고
		// 다음 수명 처리에서 제거되도록 한다.
		Particle->RelativeTime = 1.0f;
		return;
	}

	FBeamParticlePayload Payload;

	Payload.SourcePosition = ToSimulationPosition(
	    SourceModule->SourcePosition);

	Payload.TargetPosition = ToSimulationPosition(
	    TargetModule->TargetPosition);

	WriteBeamPayload(*Particle, Payload);

	// Beam 위치의 대표값으로 Source를 사용.
	Particle->Location = Payload.SourcePosition;
	Particle->OldLocation = Payload.SourcePosition;

	// 이번 Beam은 Source/Target으로 위치를 제어한다.
	Particle->Velocity = FVector::ZeroVector;
	Particle->BaseVelocity = FVector::ZeroVector;
}

void FParticleBeamEmitterInstance::IntegrateParticles(
    float DeltaTime)
{
	if (!HasValidBeamSetup())
	{
		return;
	}

	for (int32 Index = 0; Index < ActiveParticles; ++Index)
	{
		const uint16 Slot = ParticleIndices[Index];

		auto* Particle = reinterpret_cast<FBaseParticle*>(
		    ParticleData + Slot * ParticleStride);

		if (Particle->RelativeTime >= 1.0f)
		{
			continue;
		}

		const bool bJustSpawned =
		    (Particle->Flags & STATE_Particle_JustSpawned) != 0;

		Particle->Flags &= ~STATE_Particle_JustSpawned;

		if (Particle->Flags &
		    (STATE_Particle_Freeze |
		     STATE_Particle_FreezeTranslation))
		{
			continue;
		}

		FBeamParticlePayload Payload =
		    ReadBeamPayload(*Particle);

		// 새 파티클은 PostSpawn에서 이미 평가했다.
		if (!bJustSpawned)
		{
			if (!SourceModule->bLockOnSpawn)
			{
				Payload.SourcePosition = ToSimulationPosition(
				    SourceModule->SourcePosition);
			}

			if (!TargetModule->bLockOnSpawn)
			{
				Payload.TargetPosition = ToSimulationPosition(
				    TargetModule->TargetPosition);
			}
		}

		Particle->OldLocation = Particle->Location;
		Particle->Location = Payload.SourcePosition;

		WriteBeamPayload(*Particle, Payload);
	}
}

bool FParticleBeamEmitterInstance::HasValidBeamSetup() const
{
	const int32 PayloadBytes =
	    static_cast<int32>(sizeof(FBeamParticlePayload));

	return Component &&
	       ParticleData &&
	       BeamTypeData && BeamTypeData->bEnabled &&
	       SourceModule && SourceModule->bEnabled &&
	       TargetModule && TargetModule->bEnabled &&
	       BeamPayloadOffset >=
	           static_cast<int32>(sizeof(FBaseParticle)) &&
	       BeamPayloadOffset <= ParticleSize - PayloadBytes;
}

bool FParticleBeamEmitterInstance::HasValidNoiseSetup() const
{
	return NoiseModule &&
	       NoiseModule->bEnabled &&
	       NoisePayloadOffset >=
	           static_cast<int32>(sizeof(FBaseParticle)) &&
	       NoisePayloadOffset <= ParticleSize -
	                                 static_cast<int32>(sizeof(FBeamNoisePayload));
}

void FParticleBeamEmitterInstance::BuildBeamPoints(
    const FBaseParticle& Particle,
    TArray<FParticleStripPoint>& OutPoints) const
{
	OutPoints.Reset();

	if (!HasValidBeamSetup() || Particle.RelativeTime >= 1.0f)
	{
		return;
	}

	const FBeamParticlePayload Beam =
	    ReadBeamPayload(Particle);

	const FVector Delta =
	    Beam.TargetPosition - Beam.SourcePosition;

	const float LengthSquared = Delta.SizeSquared();
	const float Width = std::abs(Particle.Size.X);

	if (LengthSquared <= 1.0e-8f || Width <= 0.0f)
	{
		return;
	}

	FBeamNoisePayload Noise{};
	bool bUseNoise = false;

	if (HasValidNoiseSetup())
	{
		std::memcpy(
		    &Noise,
		    reinterpret_cast<const uint8*>(&Particle) +
		        NoisePayloadOffset,
		    sizeof(Noise));

		bUseNoise =
		    Noise.SegmentCount >= 1 &&
		    Noise.SegmentCount <= MaxBeamNoiseSegments;
	}

	int32 SegmentCount = std::clamp(
	    BeamTypeData->SegmentCount,
	    1,
	    65534);

	if (bUseNoise)
	{
		// 모든 Noise 제어점을 실제 중심점 목록에 포함한다.
		// 렌더링 구간 수를 Noise 구간 수의 배수로 올린다.
		const int32 NoiseSegments = Noise.SegmentCount;

		int32 Subdivisions =
		    (SegmentCount + NoiseSegments - 1) / NoiseSegments;

		Subdivisions = std::clamp(
		    Subdivisions,
		    1,
		    65534 / NoiseSegments);

		SegmentCount = NoiseSegments * Subdivisions;
	}

	const FVector Direction =
	    Delta / std::sqrt(LengthSquared);

	// 카메라와 무관한 Beam 수직 평면의 두 축.
	const FVector Reference =
	    std::abs(Direction.Z) < 0.9f
	        ? FVector(0.0f, 0.0f, 1.0f)
	        : FVector(0.0f, 1.0f, 0.0f);

	const FVector AxisU =
	    FVector::Cross(Direction, Reference).Normalized();

	const FVector AxisV =
	    FVector::Cross(Direction, AxisU).Normalized();

	const float Amplitude = bUseNoise
	                            ? std::max(NoiseModule->Amplitude, 0.0f)
	                            : 0.0f;

	OutPoints.SetNum(SegmentCount + 1);

	for (int32 Index = 0; Index <= SegmentCount; ++Index)
	{
		const float T =
		    static_cast<float>(Index) /
		    static_cast<float>(SegmentCount);

		FVector Position =
		    Beam.SourcePosition + Delta * T;

		// 끝점을 정확히 유지한다.
		if (bUseNoise && Index > 0 && Index < SegmentCount)
		{
			const FVector Offset = SampleBeamNoise(
				Noise, 
				T, 
				NoiseModule->UpdateInterval,
				NoiseModule->bSmoothNoise);

			Position +=
			    (AxisU * Offset.X + AxisV * Offset.Y) *
			    Amplitude;
		}

		FParticleStripPoint Point{};
		Point.Position = Position;
		Point.Width = Width;
		Point.Color = Particle.Color;

		OutPoints[Index] = Point;
	}

	// 굴곡이 생겼으므로 직선 길이 대신 실제 중심선 누적 길이 사용.
	float TotalLength = 0.0f;
	OutPoints[0].U = 0.0f;

	for (int32 Index = 1; Index < OutPoints.Num(); ++Index)
	{
		TotalLength += FVector::Distance(
		    OutPoints[Index - 1].Position,
		    OutPoints[Index].Position);

		// 일단 누적 거리를 저장한다.
		OutPoints[Index].U = TotalLength;
	}

	const float TileDistance =
	    BeamTypeData->TextureTileDistance;

	if (TileDistance > 0.0f)
	{
		for (FParticleStripPoint& Point : OutPoints)
		{
			Point.U /= TileDistance;
		}
	}
	else if (TotalLength > 0.0f)
	{
		for (FParticleStripPoint& Point : OutPoints)
		{
			Point.U /= TotalLength;
		}
	}
}

FVector FParticleBeamEmitterInstance::ToSimulationPosition(
    const FVector& ComponentLocalPosition) const
{
	if (bUseLocalSpace)
	{
		return ComponentLocalPosition;
	}

	return Component->GetWorldMatrix().TransformPosition(
	    ComponentLocalPosition);
}

FBeamParticlePayload
FParticleBeamEmitterInstance::ReadBeamPayload(
    const FBaseParticle& Particle) const
{
	FBeamParticlePayload Payload;

	std::memcpy(
	    &Payload,
	    reinterpret_cast<const uint8*>(&Particle) +
	        BeamPayloadOffset,
	    sizeof(Payload));

	return Payload;
}

void FParticleBeamEmitterInstance::WriteBeamPayload(
    FBaseParticle& Particle,
    const FBeamParticlePayload& Payload) const
{
	std::memcpy(
	    reinterpret_cast<uint8*>(&Particle) + BeamPayloadOffset,
	    &Payload,
	    sizeof(Payload));
}


void FParticleRibbonEmitterInstance::InitParameters(
    UParticleEmitter* InTemplate,
    UParticleSystemComponent* InComponent)
{
	FParticleEmitterInstance::InitParameters(
	    InTemplate,
	    InComponent);

	RibbonTypeData = CurrentLODLevel
	                     ? Cast<UParticleModuleTypeDataRibbon>(
	                           CurrentLODLevel->TypeDataModule)
	                     : nullptr;
}

FDynamicEmitterDataBase*
FParticleBeamEmitterInstance::GetDynamicData() const
{
	if (!HasValidBeamSetup() || ActiveParticles <= 0)
	{
		return nullptr;
	}

	FDynamicBeamEmitterReplayData Replay;

	Replay.MaterialInterface = GetCurrentMaterial();
	Replay.bUseLocalSpace = bUseLocalSpace;
	Replay.ActiveParticleCount = ActiveParticles;

	TArray<FParticleStripPoint> BeamPoints;

	for (int32 Index = 0; Index < ActiveParticles; ++Index)
	{
		const uint16 Slot = ParticleIndices[Index];

		const auto* Particle =
		    reinterpret_cast<const FBaseParticle*>(
		        ParticleData + Slot * ParticleStride);

		BuildBeamPoints(*Particle, BeamPoints);

		if (BeamPoints.Num() < 2)
		{
			continue;
		}

		FParticleStripRange Range{};
		Range.FirstPoint = Replay.Points.Num();
		Range.PointCount = BeamPoints.Num();

		for (const FParticleStripPoint& Point : BeamPoints)
		{
			Replay.Points.Add(Point);
		}

		Replay.Strips.Add(Range);
	}

	if (Replay.Strips.Num() == 0)
	{
		return nullptr;
	}

	return new FDynamicBeamEmitterData(std::move(Replay));
}

void FParticleBeamEmitterInstance::UpdateBoundingBox()
{
	if (!HasValidBeamSetup())
	{
		ParticleBoundingBox = FBox{};
		return;
	}

	if (Component->Template &&
	    Component->Template->bUseFixedRelativeBoundingBox)
	{
		return;
	}

	const FMatrix LocalToWorld = Component->GetWorldMatrix();

	FVector MinValue(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector MaxValue(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	bool bHasBounds = false;
	TArray<FParticleStripPoint> BeamPoints;

	for (int32 Index = 0; Index < ActiveParticles; ++Index)
	{
		const uint16 Slot = ParticleIndices[Index];

		const auto* Particle =
		    reinterpret_cast<const FBaseParticle*>(
		        ParticleData + Slot * ParticleStride);

		BuildBeamPoints(*Particle, BeamPoints);

		for (const FParticleStripPoint& Point : BeamPoints)
		{
			const FVector WorldPosition = bUseLocalSpace
			                                  ? LocalToWorld.TransformPosition(Point.Position)
			                                  : Point.Position;

			const float Radius = Point.Width * 0.5f;

			MinValue.X = std::min(
			    MinValue.X, WorldPosition.X - Radius);
			MinValue.Y = std::min(
			    MinValue.Y, WorldPosition.Y - Radius);
			MinValue.Z = std::min(
			    MinValue.Z, WorldPosition.Z - Radius);

			MaxValue.X = std::max(
			    MaxValue.X, WorldPosition.X + Radius);
			MaxValue.Y = std::max(
			    MaxValue.Y, WorldPosition.Y + Radius);
			MaxValue.Z = std::max(
			    MaxValue.Z, WorldPosition.Z + Radius);

			bHasBounds = true;
		}
	}

	if (bHasBounds)
	{
		ParticleBoundingBox = FBox{ MinValue, MaxValue };
	}
	else
	{
		const FVector Origin = Component->GetWorldLocation();
		ParticleBoundingBox = FBox{ Origin, Origin };
	}
}

#include "EnginePCH.h"

#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/Modules/ParticleModuleTypeDataRibbon.h"

#include "Components/ParticleSystemComponent.h"
#include "UObject/Casts.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <cfloat>
#include <utility>

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

	RibbonPayloadOffset = InvalidPayloadOffset;

	if (RibbonTypeData)
	{
		if (const int32* Offset =
		        InTemplate->ModuleOffsetMap.Find(RibbonTypeData))
		{
			RibbonPayloadOffset = *Offset;
		}
	}

	ResetTrailTracking();

	if (!HasValidRibbonSetup())
	{
		bHaltSpawning = true;

		HTR_LOG(
		    Error,
		    "Ribbon requires world space, one trail, "
		    "at least two slots and a valid TypeData payload.");
	}
}

bool FParticleRibbonEmitterInstance::HasValidRibbonSetup() const
{
	return Component &&
	       SpriteTemplate &&
	       ParticleData &&
	       ParticleIndices &&
	       RibbonTypeData &&
	       RibbonTypeData->bEnabled &&
	       RibbonTypeData->MaxTrailCount == 1 &&
	       !bUseLocalSpace &&
	       MaxActiveParticles >= 2 &&
	       RibbonPayloadOffset >=
	           static_cast<int32>(sizeof(FBaseParticle)) &&
	       RibbonPayloadOffset <= ParticleSize -
	                                  static_cast<int32>(sizeof(FRibbonParticlePayload));
}

void FParticleRibbonEmitterInstance::ResetTrailTracking()
{
	HeadSlot = InvalidRibbonSlot;
	TailSlot = InvalidRibbonSlot;

	bHasPreviousSource = false;
	PreviousSource = FVector::ZeroVector;

	DistanceRemainder = 0.0;
	TotalSourceDistance = 0.0;

	PendingSpawnPosition = FVector::ZeroVector;
	PendingSpawnDistance = 0.0;
}

bool FParticleRibbonEmitterInstance::TryBuildSourcePoint(
    FParticleStripPoint& OutPoint) const
{
	OutPoint = FParticleStripPoint{};

	if (!HasValidRibbonSetup() ||
	    RibbonTypeData->bClipSourceSegment ||
	    ActiveParticles <= 0 ||
	    !bHasPreviousSource ||
	    bHaltSpawning ||
	    bEmitterIsDone)
	{
		return false;
	}

	if (TailSlot < 0 || TailSlot >= MaxActiveParticles)
	{
		assert(false && "Invalid ribbon tail.");
		return false;
	}

	const FBaseParticle* TailParticle =
	    GetParticleBySlot(TailSlot);

	// Spawn()에서 마지막으로 처리한 Source 위치.
	// ReplayData와 Bounds가 같은 시뮬레이션 시점의 위치를 사용한다.
	const FVector SourcePosition = PreviousSource;

	const float SegmentLength = FVector::Distance(
	    TailParticle->Location,
	    SourcePosition);

	// Source와 마지막 파티클이 겹치면 추가할 구간이 없다.
	if (!std::isfinite(SegmentLength) ||
	    SegmentLength <= 1.0e-6f)
	{
		return false;
	}

	OutPoint.Position = SourcePosition;

	// 이번 단계에서는 마지막 실제 파티클의 속성을 이어받는다.
	OutPoint.Width = std::abs(TailParticle->Size.X);
	OutPoint.Color = TailParticle->Color;

	const float TileDistance =
	    RibbonTypeData->TextureTileDistance;

	if (TileDistance > 0.0f)
	{
		OutPoint.U = static_cast<float>(
		    TotalSourceDistance /
		    static_cast<double>(TileDistance));
	}

	return true;
}

FBaseParticle* FParticleRibbonEmitterInstance::GetParticleBySlot(
    int32 Slot)
{
	assert(Slot >= 0 && Slot < MaxActiveParticles);

	return reinterpret_cast<FBaseParticle*>(
	    ParticleData + Slot * ParticleStride);
}

const FBaseParticle*
FParticleRibbonEmitterInstance::GetParticleBySlot(
    int32 Slot) const
{
	assert(Slot >= 0 && Slot < MaxActiveParticles);

	return reinterpret_cast<const FBaseParticle*>(
	    ParticleData + Slot * ParticleStride);
}

FRibbonParticlePayload
FParticleRibbonEmitterInstance::ReadRibbonPayload(
    int32 Slot) const
{
	FRibbonParticlePayload Payload;

	const uint8* Address =
	    reinterpret_cast<const uint8*>(GetParticleBySlot(Slot)) +
	    RibbonPayloadOffset;

	std::memcpy(&Payload, Address, sizeof(Payload));

	return Payload;
}

void FParticleRibbonEmitterInstance::WriteRibbonPayload(
    int32 Slot,
    const FRibbonParticlePayload& Payload)
{
	uint8* Address =
	    reinterpret_cast<uint8*>(GetParticleBySlot(Slot)) +
	    RibbonPayloadOffset;

	std::memcpy(Address, &Payload, sizeof(Payload));
}

int32 FParticleRibbonEmitterInstance::FindActiveIndex(
    int32 Slot) const
{
	for (int32 Index = 0; Index < ActiveParticles; ++Index)
	{
		if (ParticleIndices[Index] == Slot)
		{
			return Index;
		}
	}

	return -1;
}

void FParticleRibbonEmitterInstance::KillParticle(int32 Index)
{
	if (Index < 0 || Index >= ActiveParticles)
	{
		return;
	}

	const int32 Slot = ParticleIndices[Index];
	const FRibbonParticlePayload Link = ReadRibbonPayload(Slot);

	if (Link.PrevSlot != InvalidRibbonSlot)
	{
		FRibbonParticlePayload Prev =
		    ReadRibbonPayload(Link.PrevSlot);

		Prev.NextSlot = Link.NextSlot;
		WriteRibbonPayload(Link.PrevSlot, Prev);
	}
	else
	{
		HeadSlot = Link.NextSlot;
	}

	if (Link.NextSlot != InvalidRibbonSlot)
	{
		FRibbonParticlePayload Next =
		    ReadRibbonPayload(Link.NextSlot);

		Next.PrevSlot = Link.PrevSlot;
		WriteRibbonPayload(Link.NextSlot, Next);
	}
	else
	{
		TailSlot = Link.PrevSlot;
	}

	// 연결을 먼저 복구한 뒤 활성 배열에서 제거.
	FParticleEmitterInstance::KillParticle(Index);

	if (ActiveParticles == 0)
	{
		HeadSlot = InvalidRibbonSlot;
		TailSlot = InvalidRibbonSlot;
	}
}

void FParticleRibbonEmitterInstance::KillParticlesForced()
{
	FParticleEmitterInstance::KillParticlesForced();
	ResetTrailTracking();
}

void FParticleRibbonEmitterInstance::Rewind()
{
	// Ribbon의 Rewind는 기존 경로도 비우는 것으로 정의.
	KillParticlesForced();
	FParticleEmitterInstance::Rewind();
}

void FParticleRibbonEmitterInstance::PostSpawn(
    FBaseParticle* Particle,
    float InterpolationPercentage,
    float SpawnTime)
{
	FParticleEmitterInstance::PostSpawn(
	    Particle,
	    InterpolationPercentage,
	    SpawnTime);

	// 기본 PostSpawn의 이동 보간을 덮어쓴다.
	// Ribbon은 우리가 계산한 정확한 경로 샘플 위치를 사용한다.
	Particle->Location = PendingSpawnPosition;
	Particle->OldLocation = PendingSpawnPosition;

	Particle->Velocity = FVector::ZeroVector;
	Particle->BaseVelocity = FVector::ZeroVector;

	// PostSpawn 호출 시점에는 아직 ActiveParticles 증가 전이다.
	const int32 NewSlot = ParticleIndices[ActiveParticles];

	FRibbonParticlePayload Link{};
	Link.PrevSlot = TailSlot;
	Link.NextSlot = InvalidRibbonSlot;
	Link.DistanceFromStart = PendingSpawnDistance;

	WriteRibbonPayload(NewSlot, Link);

	if (TailSlot != InvalidRibbonSlot)
	{
		FRibbonParticlePayload Tail =
		    ReadRibbonPayload(TailSlot);

		Tail.NextSlot = NewSlot;
		WriteRibbonPayload(TailSlot, Tail);
	}
	else
	{
		HeadSlot = NewSlot;
	}

	TailSlot = NewSlot;
}

bool FParticleRibbonEmitterInstance::SpawnRibbonPoint(
    const FVector& WorldPosition,
    float SpawnAge,
    double Distance)
{
	if (!HasValidRibbonSetup())
	{
		return false;
	}

	// 용량이 가득 차면 가장 오래된 점을 제거한다.
	if (ActiveParticles >= MaxActiveParticles)
	{
		const int32 HeadActiveIndex = FindActiveIndex(HeadSlot);

		if (HeadActiveIndex < 0)
		{
			assert(false && "Ribbon head is not active.");
			return false;
		}

		KillParticle(HeadActiveIndex);
	}

	PendingSpawnPosition = WorldPosition;
	PendingSpawnDistance = Distance;

	const int32 PreviousCount = ActiveParticles;

	SpawnParticles(
	    1,
	    std::max(SpawnAge, 0.0f),
	    0.0f,
	    WorldPosition,
	    FVector::ZeroVector);

	if (ActiveParticles != PreviousCount + 1)
	{
		return false;
	}

	// 긴 프레임에서는 생성 시점부터 이미 수명이 끝났을 수 있다.
	const int32 NewActiveIndex = ActiveParticles - 1;
	const int32 NewSlot = ParticleIndices[NewActiveIndex];

	if (GetParticleBySlot(NewSlot)->RelativeTime >= 1.0f)
	{
		KillParticle(NewActiveIndex);
	}

	return true;
}

void FParticleRibbonEmitterInstance::Spawn(float DeltaTime)
{
	if (!HasValidRibbonSetup())
	{
		return;
	}

	const FVector CurrentSource = Component->GetWorldLocation();

	if (bEmitterIsDone || bHaltSpawning)
	{
		// 중단 중 이동한 경로를 재개 시 한꺼번에 생성하지 않는다.
		bHasPreviousSource = false;
		DistanceRemainder = 0.0;
		return;
	}

	if (DeltaTime <= 0.0f)
	{
		return;
	}

	if (!bHasPreviousSource || ActiveParticles == 0)
	{
		// 첫 시작 또는 이전 경로가 모두 사라진 경우.
		// 중단 후 재개 시에는 남아 있던 이전 경로도 제거한다.
		KillParticlesForced();

		PreviousSource = CurrentSource;
		bHasPreviousSource = true;

		SpawnRibbonPoint(CurrentSource, 0.0f, 0.0);
		return;
	}

	const FVector StartPosition = PreviousSource;
	const FVector FrameDelta = CurrentSource - StartPosition;

	const double FrameDistance =
	    static_cast<double>(FrameDelta.Size());

	PreviousSource = CurrentSource;

	if (!std::isfinite(FrameDistance) || FrameDistance <= 1.0e-8)
	{
		return;
	}

	const double Spacing = std::max(
	    static_cast<double>(RibbonTypeData->MinSpawnDistance),
	    0.001);

	// 설정 변경으로 이전 잔여 거리가 새 간격보다 커진 경우도 정규화.
	DistanceRemainder = std::fmod(DistanceRemainder, Spacing);

	const double OldRemainder = DistanceRemainder;
	const double CombinedDistance = OldRemainder + FrameDistance;

	const double RequestedSamples =
	    std::floor(CombinedDistance / Spacing);

	const double FrameStartDistance = TotalSourceDistance;

	TotalSourceDistance += FrameDistance;
	DistanceRemainder = std::fmod(CombinedDistance, Spacing);

	if (RequestedSamples < 1.0)
	{
		return;
	}

	// 한 프레임의 작업량을 최대 슬롯 수로 제한.
	// 그보다 많이 필요하면 가장 최근 구간의 샘플만 남긴다.
	const int32 SampleCount = static_cast<int32>(
	    std::min(
	        RequestedSamples,
	        static_cast<double>(MaxActiveParticles)));

	const double SkippedSamples =
	    RequestedSamples - static_cast<double>(SampleCount);

	const double FirstDistance =
	    Spacing - OldRemainder + SkippedSamples * Spacing;

	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		const double DistanceAlongFrame = std::clamp(
		    FirstDistance + static_cast<double>(Index) * Spacing,
		    0.0,
		    FrameDistance);

		const float T = static_cast<float>(
		    DistanceAlongFrame / FrameDistance);

		const FVector SamplePosition =
		    StartPosition + FrameDelta * T;

		// 프레임 내 생성 시점 이후 경과 시간.
		const float SpawnAge = DeltaTime * (1.0f - T);

		if (!SpawnRibbonPoint(
		        SamplePosition,
		        SpawnAge,
		        FrameStartDistance + DistanceAlongFrame))
		{
			break;
		}
	}
}

void FParticleRibbonEmitterInstance::IntegrateParticles(
    float DeltaTime)
{
	for (int32 Index = 0; Index < ActiveParticles; ++Index)
	{
		FBaseParticle* Particle =
		    GetParticleBySlot(ParticleIndices[Index]);

		Particle->OldLocation = Particle->Location;

		Particle->Velocity = FVector::ZeroVector;
		Particle->BaseVelocity = FVector::ZeroVector;

		Particle->Flags &= ~STATE_Particle_JustSpawned;
	}
}

FDynamicEmitterDataBase*
FParticleRibbonEmitterInstance::GetDynamicData() const
{
	if (!HasValidRibbonSetup() || ActiveParticles <= 0)
	{
		return nullptr;
	}

	FDynamicRibbonEmitterReplayData Replay;
	Replay.MaterialInterface = GetCurrentMaterial();
	Replay.bUseLocalSpace = false;
	Replay.ActiveParticleCount = ActiveParticles;

	Replay.Points.Reserve(ActiveParticles + 1);

	const float TileDistance =
	    RibbonTypeData->TextureTileDistance;

	int32 Slot = HeadSlot;
	int32 VisitedCount = 0;

	float CurrentLength = 0.0f;
	FVector PreviousPosition = FVector::ZeroVector;

	while (Slot != InvalidRibbonSlot &&
	       VisitedCount < ActiveParticles)
	{
		if (Slot < 0 || Slot >= MaxActiveParticles)
		{
			assert(false && "Invalid ribbon slot.");
			return nullptr;
		}

		const FBaseParticle* Particle = GetParticleBySlot(Slot);
		const FRibbonParticlePayload Link = ReadRibbonPayload(Slot);

		FParticleStripPoint Point{};
		Point.Position = Particle->Location;
		Point.Width = std::abs(Particle->Size.X);
		Point.Color = Particle->Color;

		if (VisitedCount > 0)
		{
			CurrentLength += FVector::Distance(
			    PreviousPosition,
			    Point.Position);
		}

		if (TileDistance > 0.0f)
		{
			// 오래된 점이 사라져도 남은 점의 UV를 변경하지 않는다.
			Point.U = static_cast<float>(
			    Link.DistanceFromStart /
			    static_cast<double>(TileDistance));
		}
		else
		{
			// 나중에 현재 전체 길이로 정규화한다.
			Point.U = CurrentLength;
		}

		Replay.Points.Add(Point);
		PreviousPosition = Point.Position;

		Slot = Link.NextSlot;
		++VisitedCount;
	}

	// 연결 순환이나 누락을 조기에 발견한다.
	if (Slot != InvalidRibbonSlot || VisitedCount != ActiveParticles)
	{
		assert(false && "Broken ribbon chain.");
		return nullptr;
	}

	FParticleStripPoint SourcePoint{};

	if (TryBuildSourcePoint(SourcePoint))
	{
		const float SourceSegmentLength = FVector::Distance(
		    PreviousPosition,
		    SourcePoint.Position);

		CurrentLength += SourceSegmentLength;

		if (TileDistance <= 0.0f)
		{
			// 정규화 전 누적 길이.
			// 아래의 기존 정규화 루프에서 전체 길이로 나눈다.
			SourcePoint.U = CurrentLength;
		}

		Replay.Points.Add(SourcePoint);
	}

	if (Replay.Points.Num() < 2 || CurrentLength <= 1.0e-6f)
	{
		return nullptr;
	}

	if (TileDistance <= 0.0f)
	{
		for (FParticleStripPoint& Point : Replay.Points)
		{
			Point.U /= CurrentLength;
		}
	}

	FParticleStripRange Range{};
	Range.FirstPoint = 0;
	Range.PointCount = Replay.Points.Num();

	Replay.Strips.Add(Range);

	return new FDynamicRibbonEmitterData(std::move(Replay));
}

void FParticleRibbonEmitterInstance::UpdateBoundingBox()
{
	if (!HasValidRibbonSetup())
	{
		ParticleBoundingBox = FBox{};
		return;
	}

	if (Component->Template &&
	    Component->Template->bUseFixedRelativeBoundingBox)
	{
		return;
	}

	if (ActiveParticles <= 0)
	{
		const FVector Origin = Component->GetWorldLocation();
		ParticleBoundingBox = FBox{ Origin, Origin };
		return;
	}

	FVector MinValue(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector MaxValue(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	const auto ExpandBounds =
	    [&MinValue, &MaxValue](
	        const FVector& Position,
	        float Width)
	{
		const float Radius = std::abs(Width) * 0.5f;

		MinValue.X = std::min(
		    MinValue.X, Position.X - Radius);
		MinValue.Y = std::min(
		    MinValue.Y, Position.Y - Radius);
		MinValue.Z = std::min(
		    MinValue.Z, Position.Z - Radius);

		MaxValue.X = std::max(
		    MaxValue.X, Position.X + Radius);
		MaxValue.Y = std::max(
		    MaxValue.Y, Position.Y + Radius);
		MaxValue.Z = std::max(
		    MaxValue.Z, Position.Z + Radius);
	};

	// 실제 파티클.
	for (int32 Index = 0; Index < ActiveParticles; ++Index)
	{
		const FBaseParticle* Particle =
		    GetParticleBySlot(ParticleIndices[Index]);

		ExpandBounds(
		    Particle->Location,
		    Particle->Size.X);
	}

	// 렌더링용 Source 점.
	FParticleStripPoint SourcePoint{};

	if (TryBuildSourcePoint(SourcePoint))
	{
		ExpandBounds(
		    SourcePoint.Position,
		    SourcePoint.Width);
	}

	ParticleBoundingBox = FBox{ MinValue, MaxValue };
}

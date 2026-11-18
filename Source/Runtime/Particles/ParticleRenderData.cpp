#include "EnginePCH.h"
#include "ParticleRenderData.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <utility>

#include "Math/Transform.h"
#include "ParticleVertex.h"
#include "Engine/ParticleSystemSceneProxy.h"
#include "Render/Mesh.h"
#include "Render/Material.h"
#include "Render/RenderPacket.h"

FDynamicSpriteEmitterData::FDynamicSpriteEmitterData(
    FDynamicSpriteEmitterReplayData&& InSource)
    : Source(std::move(InSource))
{
	Source.eEmitterType = EDynamicEmitterType::Sprite;
}

const FDynamicSpriteEmitterReplayData&
FDynamicSpriteEmitterData::GetSource() const
{
	return Source;
}

int32 FDynamicSpriteEmitterData::GetDynamicVertexStride() const
{
	// 현재 인스턴싱 설계에서 동적으로 업로드하는 레코드의 크기.
	return static_cast<int32>(
	    sizeof(FParticleSpriteInstanceVertex));
}

FDynamicMeshEmitterData::FDynamicMeshEmitterData(
    FDynamicMeshEmitterReplayData&& InSource)
    : Source(std::move(InSource))
{
	Source.eEmitterType = EDynamicEmitterType::Mesh;
}

const FDynamicMeshEmitterReplayData&
FDynamicMeshEmitterData::GetSource() const
{
	return Source;
}

int32 FDynamicMeshEmitterData::GetDynamicVertexStride() const
{
	return static_cast<int32>(
	    sizeof(FMeshParticleInstanceVertex));
}

void FDynamicSpriteEmitterDataBase::SortParticles(
    const FVector& CameraLocation,
    const FVector& CameraForward,
    const FMatrix& LocalToWorld,
    TArray<FParticleOrder>* OutParticleOrder) const
{
	assert(OutParticleOrder);
	TArray<FParticleOrder>& ParticleOrder = *OutParticleOrder;

	const FDynamicSpriteEmitterReplayDataBase& Source = GetSource();

	assert(Source.ActiveParticleCount >= 0);

	ParticleOrder.Reset();
	ParticleOrder.Reserve(Source.ActiveParticleCount);

	for (int32 ActiveIndex = 0;
	     ActiveIndex < Source.ActiveParticleCount;
	     ++ActiveIndex)
	{
		FParticleOrder Order;
		Order.ParticleIndex = Source.GetParticleIndex(ActiveIndex);
		Order.ActiveIndex = ActiveIndex;

		if (Source.SortMode != EParticleSortMode::None)
		{
			const FBaseParticle& Particle = Source.GetParticle(ActiveIndex);

			const FVector WorldPosition =
			    Source.bUseLocalSpace
			        ? LocalToWorld.TransformPosition(Particle.Location)
			        : Particle.Location;

			const FVector ToParticle = WorldPosition - CameraLocation;

			switch (Source.SortMode)
			{
			case EParticleSortMode::DistanceToView:
				Order.SortKey = ToParticle.SizeSquared();
				break;

			case EParticleSortMode::ViewDepth:
				Order.SortKey =
				    FVector::Dot(ToParticle, CameraForward);
				break;

			default:
				assert(false);
				break;
			}

			assert(std::isfinite(Order.SortKey));
		}

		ParticleOrder.Add(Order);
	}

	if (Source.SortMode == EParticleSortMode::None ||
	    ParticleOrder.Num() < 2)
	{
		return;
	}

	// 동일한 키의 파티클은 기존 활성 목록 순서를 유지한다.
	std::stable_sort(
	    ParticleOrder.begin(),
	    ParticleOrder.end(),
	    [](const FParticleOrder& A, const FParticleOrder& B)
	    {
		    // SortKey가 큰 순서대로 정렬한다. (카메라에서 먼 순서)
		    return A.SortKey > B.SortKey;
	    });
}

FVector4 MakeSubUVRect(int32 Frame, int32 Columns, int32 Rows)
{
	Columns = std::max(Columns, 1);
	Rows = std::max(Rows, 1);
	Frame = std::clamp(Frame, 0, Columns * Rows - 1);

	// UV의 (0,0)이 텍스처 왼쪽 위라서 행 0이 맨 위, 프레임은 왼쪽→오른쪽, 위→아래로 센다.
	const float CellW = 1.0f / static_cast<float>(Columns);
	const float CellH = 1.0f / static_cast<float>(Rows);
	return FVector4(
	    static_cast<float>(Frame % Columns) * CellW,
	    static_cast<float>(Frame / Columns) * CellH,
	    CellW,
	    CellH);
}

void FDynamicSpriteEmitterData::PrepareRenderData(const FParticleSystemSceneProxy& Proxy)
{
	// 이번 단계에서 지원하는 실제 재질 타입은 UMaterial이다.
	Material = Cast<UMaterial>(Source.MaterialInterface);
	if (!Material)
	{
		Instances.Reset();
		return;
	}

	assert(Source.ActiveParticleCount >= 0);

	Instances.SetNum(Source.ActiveParticleCount);

	const FMatrix& LocalToWorld = Proxy.GetLocalToWorld();

	for (int32 ActiveIndex = 0;
	     ActiveIndex < Source.ActiveParticleCount;
	     ++ActiveIndex)
	{
		const FBaseParticle& Particle =
		    Source.GetParticle(ActiveIndex);

		FParticleSpriteInstanceVertex Instance{};

		Instance.Position =
		    Source.bUseLocalSpace
		        ? LocalToWorld.TransformPosition(Particle.Location)
		        : Particle.Location;

		Instance.Rotation = Particle.Rotation;

		Instance.Size = FVector2(
		    std::abs(Particle.Size.X * Source.Scale.X),
		    std::abs(Particle.Size.Y * Source.Scale.Y));

		Instance.Color = FVector4(
		    Particle.Color.R,
		    Particle.Color.G,
		    Particle.Color.B,
		    Particle.Color.A);

		// 기본은 전체 텍스처. 시트(가로x세로 > 1)이면 프레임 번호에 해당하는 칸만 쓴다.
		Instance.UVRect = FVector4(0.0f, 0.0f, 1.0f, 1.0f);
		const int32 Columns = std::max(Source.SubImagesHorizontal, 1);
		const int32 Rows = std::max(Source.SubImagesVertical, 1);
		if (Columns * Rows > 1)
		{
			int32 Frame = 0;
			if (const uint8* Payload = Source.GetParticlePayload(ActiveIndex, Source.SubUVPayloadOffset))
			{
				float FrameValue = 0.0f;
				std::memcpy(&FrameValue, Payload, sizeof(float));
				Frame = std::clamp(static_cast<int32>(std::floor(FrameValue)), 0, Columns * Rows - 1);
			}
			Instance.UVRect = MakeSubUVRect(Frame, Columns, Rows);
		}

		Instance.Velocity =
		    Source.bUseLocalSpace
		        ? LocalToWorld.TransformPosition(Particle.Velocity) -
		              LocalToWorld.TransformPosition(FVector::ZeroVector)
		        : Particle.Velocity;

		Instances[ActiveIndex] = std::move(Instance);
	}
}

void FDynamicSpriteEmitterData::GatherRenderPackets(
    const FParticleSystemSceneProxy& Proxy,
    FRenderQueue& RenderQueue,
    const FRenderViewContext& View) const
{
	if (!Material || Instances.Num() == 0)
		return;

	// AlphaBlend/ Additive Sprite를 대상으로 한다.
	// 에디터에서 불투명 머티리얼을 고르거나 저장된 에셋이 그런 머티리얼을 가리킬 수 있으므로,
	// assert로 멈추지 않고 이 이미터만 그리지 않는다.
	if (Material->BlendState != EBlendState::AlphaBlend &&
	    Material->BlendState != EBlendState::Additive)
	{
		return;
	}

	assert(Instances.Num() == Source.ActiveParticleCount);

	// 이 뷰에서만 사용할 정렬 결과.
	TArray<FParticleOrder> ParticleOrder;

	SortParticles(
	    View.CameraLocation,
	    View.CameraForward,
	    Proxy.GetLocalToWorld(),
	    &ParticleOrder);

	FRenderPacket Packet{};

	Packet.Material = Material;
	Packet.ParticleScreenAlignment = Source.ScreenAlignment;
	Packet.ParticleAxisLock = Source.AxisLock;

	FVector Center = FVector::ZeroVector;

	Packet.SpriteInstances.SetNum(ParticleOrder.Num());

	for (int32 SortedIndex = 0;
	     SortedIndex < ParticleOrder.Num();
	     ++SortedIndex)
	{
		const int32 ActiveIndex =
		    ParticleOrder[SortedIndex].ActiveIndex;

		assert(ActiveIndex >= 0);
		assert(ActiveIndex < Instances.Num());

		const FParticleSpriteInstanceVertex& Instance =
		    Instances[ActiveIndex];

		Packet.SpriteInstances[SortedIndex] = Instance;
		Center += Instance.Position;
	}

	Center *= 1.0f / static_cast<float>(Packet.SpriteInstances.Num());

	Packet.TranslucentSortCenter = Center;

	// 배열을 포함한 패킷을 이동한다.
	RenderQueue.Add(std::move(Packet));
}

void FDynamicMeshEmitterData::PrepareRenderData(
    const FParticleSystemSceneProxy& Proxy)
{
	Instances.Reset();

	if (!Source.Mesh || Source.ActiveParticleCount <= 0)
		return;

	Instances.SetNum(Source.ActiveParticleCount);

	for (int32 Index = 0;
	     Index < Source.ActiveParticleCount;
	     ++Index)
	{
		const FBaseParticle& Particle = Source.GetParticle(Index);

		FTransform Transform;
		Transform.Location = Particle.Location;

		constexpr float RadToDeg = 180.0f / static_cast<float>(PI);

		// payload가 없는 경우의 회전.
		Transform.Rotation = FRotator(
		    0.0f, Particle.Rotation * RadToDeg, 0.0f);

		const int32 Offset = Source.MeshRotationPayloadOffset;
		if (Offset >= static_cast<int32>(sizeof(FBaseParticle)) &&
		    Offset <= Source.ParticleStride - static_cast<int32>(sizeof(FVector)))
		{
			FVector RotationRadians;
			std::memcpy(
			    &RotationRadians,
			    Source.GetParticlePayload(Index, Offset),
			    sizeof(FVector));

			// FRotator 생성자는 Pitch(Y), Yaw(Z), Roll(X) 순서이므로 변환한다.
			Transform.Rotation = FRotator(
			    RotationRadians.Y * RadToDeg,
			    RotationRadians.Z * RadToDeg,
			    RotationRadians.X * RadToDeg);
		}

		// Source.Scale에는 이미 Component의 WorldScale이 들어온다.
		// Local Space는 아래 LocalToWorld에서 스케일을 적용한다.
		Transform.Scale = Source.bUseLocalSpace
		                      ? Particle.Size
		                      : Particle.Size * Source.Scale;

		FMeshParticleInstanceVertex& Instance = Instances[Index];

		const FMatrix ParticleTransform = Transform.GetLocalMatrix();

		// 현재 엔진은 row-vector 규약.
		Instance.Transform = Source.bUseLocalSpace
		                         ? ParticleTransform * Proxy.GetLocalToWorld()
		                         : ParticleTransform;

		const FMatrix& World = Instance.Transform;
		// 상단 3x3의 determinant.
		// 스케일 0 등으로 역행렬이 존재하지 않는 경우를 구분한다.
		const float Determinant =
		    World.M[0][0] *
		        (World.M[1][1] * World.M[2][2] -
		         World.M[1][2] * World.M[2][1]) -
		    World.M[0][1] *
		        (World.M[1][0] * World.M[2][2] -
		         World.M[1][2] * World.M[2][0]) +
		    World.M[0][2] *
		        (World.M[1][0] * World.M[2][1] -
		         World.M[1][1] * World.M[2][0]);

		const FMatrix NormalMatrix =
		    std::abs(Determinant) > 1e-8f
		        ? World.Inverse().GetTransposed()
		        : FMatrix::Identity;

		Instance.NormalRow0 = FVector4(
		    NormalMatrix.M[0][0],
		    NormalMatrix.M[0][1],
		    NormalMatrix.M[0][2],
		    0.0f);

		Instance.NormalRow1 = FVector4(
		    NormalMatrix.M[1][0],
		    NormalMatrix.M[1][1],
		    NormalMatrix.M[1][2],
		    0.0f);

		Instance.NormalRow2 = FVector4(
		    NormalMatrix.M[2][0],
		    NormalMatrix.M[2][1],
		    NormalMatrix.M[2][2],
		    0.0f);

		Instance.Color = FVector4(
		    Particle.Color.R,
		    Particle.Color.G,
		    Particle.Color.B,
		    Particle.Color.A);
	}
}

void FDynamicMeshEmitterData::GatherRenderPackets(
    const FParticleSystemSceneProxy& Proxy,
    FRenderQueue& RenderQueue,
    const FRenderViewContext& View) const
{
	if (!Source.Mesh || Instances.Num() == 0)
		return;

	assert(Instances.Num() == Source.ActiveParticleCount);

	const FStaticMeshData& MeshData = Source.Mesh->GetMeshData(0);

	UMaterial* OverrideMaterial =
	    Cast<UMaterial>(Source.MaterialInterface);

	// 1. ReplayData의 SortMode에 따라 뷰별 순서를 계산한다.
	TArray<FParticleOrder> ParticleOrder;

	SortParticles(
	    View.CameraLocation,
	    View.CameraForward,
	    Proxy.GetLocalToWorld(),
	    &ParticleOrder);

	// 2. 정렬 결과로 이 뷰에서 사용할 인스턴스 배열을 만든다.
	// 멤버 Instances는 원래 활성 파티클 순서대로 유지한다.
	TArray<FMeshParticleInstanceVertex> OrderedInstances;
	OrderedInstances.SetNum(ParticleOrder.Num());

	FVector Center = FVector::ZeroVector;

	for (int32 SortedIndex = 0;
	     SortedIndex < ParticleOrder.Num();
	     ++SortedIndex)
	{
		const int32 ActiveIndex =
		    ParticleOrder[SortedIndex].ActiveIndex;

		assert(ActiveIndex >= 0);
		assert(ActiveIndex < Instances.Num());

		const FMeshParticleInstanceVertex& Instance = Instances[ActiveIndex];
		OrderedInstances[SortedIndex] = Instance;

		Center += Instance.Transform.TransformPosition(
		    FVector::ZeroVector);
	}

	Center *= 1.0f / static_cast<float>(OrderedInstances.Num());

	// 3. 모든 Section에서 동일한 인스턴스 순서를 사용한다.
	auto AddSectionPacket =
	    [&](uint32 StartIndex,
	        uint32 IndexCount,
	        uint32 MaterialSlotIndex)
	{
		if (IndexCount == 0)
			return;

		UMaterial* Material = OverrideMaterial
		                          ? OverrideMaterial
		                          : Source.Mesh->GetMaterial(MaterialSlotIndex);

		if (!Material)
			return;

		FRenderPacket Packet{};

		Packet.Proxy = &Proxy;
		Packet.Mesh = Source.Mesh;
		Packet.Material = Material;
		Packet.LODIndex = 0;
		Packet.StartIndex = StartIndex;
		Packet.IndexCount = IndexCount;

		Packet.MeshInstances = OrderedInstances;
		Packet.TranslucentSortCenter = Center;

		RenderQueue.Add(std::move(Packet));
	};

	// 기본 도형 처럼 Section 배열이 없는 메시도 처리.
	if (MeshData.Sections.Num() == 0)
	{
		AddSectionPacket(
		    0,
		    static_cast<uint32>(MeshData.Indices.Num()),
		    0);
	}
	else
	{
		for (const FStaticMeshSection& Section : MeshData.Sections)
		{
			AddSectionPacket(
			    Section.StartIndex,
			    Section.IndexCount,
			    Section.MaterialSlotIndex);
		}
	}
}

void FDynamicStripEmitterDataBase::PrepareRenderData(
    const FParticleSystemSceneProxy& Proxy)
{
	Material = Cast<UMaterial>(GetSource().MaterialInterface);
}

void FDynamicStripEmitterDataBase::GatherRenderPackets(
    const FParticleSystemSceneProxy& Proxy,
    FRenderQueue& RenderQueue,
    const FRenderViewContext& View) const
{
	if (!Material)
		return;

	if (Material->BlendState != EBlendState::AlphaBlend &&
	    Material->BlendState != EBlendState::Additive)
	{
		return;
	}

	const FDynamicStripEmitterReplayDataBase& Source =
	    GetSource();

	const FMatrix& LocalToWorld = Proxy.GetLocalToWorld();

	for (const FParticleStripRange& Range : Source.Strips)
	{
		// 덧셈 overflow 없이 범위를 검사한다.
		if (Range.FirstPoint < 0 ||
		    Range.PointCount < 2 ||
		    Range.FirstPoint > Source.Points.Num() ||
		    Range.PointCount >
		        Source.Points.Num() - Range.FirstPoint)
		{
			continue;
		}

		FRenderPacket Packet{};
		Packet.Material = Material;
		Packet.StripPoints.Reserve(Range.PointCount);

		FVector Center = FVector::ZeroVector;

		for (int32 Index = 0; Index < Range.PointCount; ++Index)
		{
			FParticleStripPoint Point =
			    Source.Points[Range.FirstPoint + Index];

			if (Source.bUseLocalSpace)
			{
				Point.Position =
				    LocalToWorld.TransformPosition(Point.Position);
			}

			// 이번 단계의 Width는 월드 단위 폭으로 취급한다.
			Point.Width = std::abs(Point.Width);

			Center += Point.Position;
			Packet.StripPoints.Add(Point);
		}

		Packet.TranslucentSortCenter =
		    Center / static_cast<float>(Range.PointCount);

		RenderQueue.Add(std::move(Packet));
	}
}
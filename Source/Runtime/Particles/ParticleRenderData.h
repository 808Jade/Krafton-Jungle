#pragma once

#include "Particles/ParticleReplayData.h"
#include "Particles/ParticleVertex.h"
#include "Engine/PrimitiveSceneProxy.h"

class FParticleSystemSceneProxy;
class FRenderQueue;
class UMaterial;

inline constexpr int32 InvalidEmitterIndex = -1;

// 스프라이트 시트(Columns x Rows)에서 Frame번 칸의 UV 사각형(xy = 시작, zw = 크기)을 만든다.
// Frame은 0~(칸 수-1)로 잘리고, 왼쪽→오른쪽, 위→아래로 센다.
FVector4 MakeSubUVRect(int32 Frame, int32 Columns, int32 Rows);

struct FParticleOrder
{
	// PartilceData 내 실제 저장 슬롯 번호.
	uint16 ParticleIndex = 0;

	// 활성 목록과 Instances 배열에서의 위치.
	int32 ActiveIndex = 0;

	float SortKey = 0.0f;
};

struct FDynamicEmitterDataBase
{
	virtual ~FDynamicEmitterDataBase() = default;

	int32 EmitterIndex = InvalidEmitterIndex;

	virtual const FDynamicEmitterReplayDataBase& GetSource() const = 0;

	// 프레임당 1번
	virtual void PrepareRenderData(const FParticleSystemSceneProxy& Proxy) {}

	// 뷰마다
	virtual void GatherRenderPackets(
		const FParticleSystemSceneProxy& Proxy,
		FRenderQueue& RenderQueue,
		const FRenderViewContext& View
	) const {}
};

struct FDynamicSpriteEmitterDataBase : public FDynamicEmitterDataBase
{
	// 공통 ReplayData에 대한 타입 지정 접근.
	virtual const FDynamicSpriteEmitterReplayDataBase&
	GetSource() const override = 0;

	// 원본 인덱스 배열을 변경하지 않고 뷰별 정렬 순서를 출력.
	void SortParticles(
	    const FVector& CameraLocation,
	    const FVector& CameraForward,
	    const FMatrix& LocalToWorld,
	    TArray<FParticleOrder>* OutParticleOrder) const;

	virtual int32 GetDynamicVertexStride() const = 0;
};

struct FDynamicSpriteEmitterData : public FDynamicSpriteEmitterDataBase
{
public:
	explicit FDynamicSpriteEmitterData(
	    FDynamicSpriteEmitterReplayData&& InSource);

	const FDynamicSpriteEmitterReplayData& GetSource() const override;

	int32 GetDynamicVertexStride() const override;

	void PrepareRenderData(const FParticleSystemSceneProxy& Proxy) override;
	void GatherRenderPackets(
		const FParticleSystemSceneProxy& Proxy,
		FRenderQueue& RenderQueue,
		const FRenderViewContext& View) const override;

private:
	FDynamicSpriteEmitterReplayData Source;
	UMaterial* Material = nullptr;
	TArray<FParticleSpriteInstanceVertex> Instances;
};

struct FDynamicMeshEmitterData : public FDynamicSpriteEmitterDataBase
{
public:
	explicit FDynamicMeshEmitterData(
	    FDynamicMeshEmitterReplayData&& InSource);

	const FDynamicMeshEmitterReplayData& GetSource() const override;

	int32 GetDynamicVertexStride() const override;

	void PrepareRenderData(const FParticleSystemSceneProxy& Proxy) override;
	void GatherRenderPackets(
	    const FParticleSystemSceneProxy& Proxy,
	    FRenderQueue& RenderQueue,
	    const FRenderViewContext& View) const override;

private:
	FDynamicMeshEmitterReplayData Source;
	TArray<FMeshParticleInstanceVertex> Instances;
};

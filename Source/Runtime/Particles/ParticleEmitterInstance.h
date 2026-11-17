#pragma once
#include "ParticleTypes.h"
#include "ParticleReplayData.h"
#include "ParticleRenderData.h"
#include "ParticleBeamData.h"
#include "ParticleRibbonData.h"
#include "Math/Box.h"
#include "Math/Matrix.h"

class UParticleSystemComponent;
class UParticleEmitter;
class UParticleLODLevel;
class UParticleModuleTypeDataMesh;
class UParticleModuleTypeDataBeam;
class UParticleModuleTypeDataRibbon;
class UParticleModuleBeamTarget;
class UParticleModuleBeamSource;
class UParticleModuleBeamNoise;
class UMaterialInterface;
class UStaticMesh;

struct FParticleEmitterInstance
{
protected:
	void PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity);
	virtual void PostSpawn(FBaseParticle* Particle, float InterpolationPercentage, float SpawnTime);

	/** 모든 이미터 타입에 공통인 ReplayData 필드를 채운다. 파생 클래스의 override에서 먼저 호출 */
	virtual bool FillReplayData(FDynamicEmitterReplayDataBase& OutDynamicEmitterReplayDataBase) const;
	void FillSpriteReplaySettings(FDynamicSpriteEmitterReplayDataBase& OutData) const;

	UMaterialInterface* GetCurrentMaterial() const;

public:
	FParticleEmitterInstance() = default;
	FParticleEmitterInstance(const FParticleEmitterInstance&) = delete;
	FParticleEmitterInstance& operator=(const FParticleEmitterInstance&) = delete;

	virtual ~FParticleEmitterInstance();
	/** 이미터 타입에 맞는 DynamicEmitterData를 생성해 반환. 호출자가 delete 책임. 베이스는 nullptr */
	virtual FDynamicEmitterDataBase* GetDynamicData() const;
	virtual void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent);

	virtual void Spawn(float DeltaTime);
	void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation,
	                    const FVector& InitialVelocity);

	virtual void KillParticle(int32 Index);
	virtual void KillParticlesForced();
	void Init(int32 InMaxParticles, int32 InParticleSize);
	void Release();
	virtual void Rewind();
	bool HasCompleted() const;
	void UpdateModules(float DeltaTime);

	void Tick(float DeltaTime);
	void TickEmitterTime(float DeltaTime);
	void AgeAndKillParticles(float DeltaTime);
	void ResetParticleParameters();
	virtual void IntegrateParticles(float DeltaTime);

	virtual void SetCurrentLODIndex(int32 LODIndex);

	void UpdateTransforms();

	virtual void UpdateBoundingBox();
	const FBox& GetBoundingBox() const { return ParticleBoundingBox; }

	// Asset
	UParticleEmitter* SpriteTemplate = nullptr;

	// Owner
	UParticleSystemComponent* Component = nullptr;
	int32 CurrentLODLevelIndex = 0;
	UParticleLODLevel* CurrentLODLevel = nullptr;

	UMaterialInterface* CurrentMaterial = nullptr;

	bool bUseLocalSpace = false;
	FMatrix EmitterToSimulation = FMatrix::Identity;
	FMatrix SimulationToWorld = FMatrix::Identity;
	FVector Location = FVector(0.0f, 0.0f, 0.0f);
	FVector OldLocation = FVector(0.0f, 0.0f, 0.0f);


	// Particle
	/** 파티클 데이터 배열(메모리 블록 시작 주소). 해제할 대상은 이 포인터 하나 */
	uint8* ParticleData = nullptr;
	/** 파티클 인덱스 배열. 살아 있는 파티클의 슬롯 번호 명단 (블록 끝부분, 따로 할당하지 않음) */
	uint16* ParticleIndices = nullptr;
	/** 인스턴스 데이터 배열. 파티클마다가 아니라 인스턴스당 하나인 모듈 데이터 */
	uint8* InstanceData = nullptr;
	/** 인스턴스 데이터 배열의 크기(바이트) */
	int32 InstancePayloadSize = 0;
	/** 파티클 안에서 모듈 Payload가 시작되는 위치(바이트) */
	int32 PayloadOffset = 0;
	/** 파티클 한 개의 전체 크기(바이트). FBaseParticle + 모듈 Payload 합 */
	int32 ParticleSize = 0;
	/** ParticleData 배열에서 파티클 사이 간격(바이트). ParticleSize를 16의 배수로 올린 값 */
	int32 ParticleStride = 0;
	/** 이미터에서 현재 살아 있는 파티클 수 */
	int32 ActiveParticles = 0;
	/** 계속 증가하는 생성 카운터. Flags 하위 비트에 생성 순번으로 기록 */
	uint32 ParticleCounter = 0;
	/** 파티클 데이터 배열에 담을 수 있는 최대 파티클 수(슬롯 개수) */
	int32 MaxActiveParticles = 0;
	/** 스폰하고 남은 시간의 소수 부분 */
	float SpawnFraction = 0.0f;
	FBox ParticleBoundingBox{};

	// Emitter Time
	float EmitterTime = 0.0f;     // 현재 루프 안에서 지난 시간(0 ~Duration)
	float PrevEmitterTime = 0.0f; // 이전 틱의 EmitterTime.나중에 B의 GetBurstBatches(Prev, Curr, Duration)에 그대로 넘김
	int32 LoopCount = 0;          // 끝난 루프 수
	float EmitterDuration = 1.0f; // 한 루프 길이(초).Required 값으로 채움
	int32 EmitterLoops = 0;       // 반복 횟수. 0이면 무한.Required 값으로 채움
	bool bEmitterIsDone = false;  // 정해진 루프를 다 돌았음 → 더 이상 스폰하지 않음
	bool bHaltSpawning = false;   // 스폰만 멈춤.
};

struct FParticleSpriteEmitterInstance : public FParticleEmitterInstance
{
public:
	virtual FDynamicEmitterDataBase* GetDynamicData() const override;

protected:
	virtual bool FillReplayData(FDynamicEmitterReplayDataBase& OutDynamicEmitterReplayDataBase) const override;
};

struct FParticleMeshEmitterInstance : public FParticleEmitterInstance
{
public:
	virtual FDynamicEmitterDataBase* GetDynamicData() const override;
	virtual void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent) override;
	virtual void SetCurrentLODIndex(int32 LODIndex) override;

	UParticleModuleTypeDataMesh* MeshTypeData = nullptr;
	int32 MeshRotationOffset = InvalidPayloadOffset;

protected:
	virtual bool FillReplayData(FDynamicEmitterReplayDataBase& OutDynamicEmitterReplayDataBase) const override;
};

struct FParticleBeamEmitterInstance
    : public FParticleEmitterInstance
{
public:
	void InitParameters(
	    UParticleEmitter* InTemplate,
	    UParticleSystemComponent* InComponent) override;
	void SetCurrentLODIndex(int32 LODIndex) override;

	void IntegrateParticles(float DeltaTime) override;
	void UpdateBoundingBox() override;

	FDynamicEmitterDataBase* GetDynamicData() const override;

	UParticleModuleTypeDataBeam* BeamTypeData = nullptr;
	UParticleModuleBeamSource* SourceModule = nullptr;
	UParticleModuleBeamTarget* TargetModule = nullptr;
	UParticleModuleBeamNoise* NoiseModule = nullptr;

	int32 BeamPayloadOffset = InvalidPayloadOffset;
	int32 NoisePayloadOffset = InvalidPayloadOffset;

protected:
	void PostSpawn(
	    FBaseParticle* Particle,
	    float InterpolationPercentage,
	    float SpawnTime) override;

private:
	void CacheBeamModules();
	bool HasValidBeamSetup() const;
	bool HasValidNoiseSetup() const;

	void BuildBeamPoints(
	    const FBaseParticle& Particle,
	    TArray<FParticleStripPoint>& OutPoints) const;

	FVector ToSimulationPosition(
	    const FVector& ComponentLocalPosition) const;

	FBeamParticlePayload ReadBeamPayload(
	    const FBaseParticle& Particle) const;

	void WriteBeamPayload(
	    FBaseParticle& Particle,
	    const FBeamParticlePayload& Payload) const;
};

struct FParticleRibbonEmitterInstance
    : public FParticleEmitterInstance
{
public:
	void InitParameters(
	    UParticleEmitter* InTemplate,
	    UParticleSystemComponent* InComponent) override;
	void SetCurrentLODIndex(int32 LODIndex) override;

	void Spawn(float DeltaTime) override;
	void IntegrateParticles(float DeltaTime) override;

	void KillParticle(int32 Index) override;
	void KillParticlesForced() override;
	void Rewind() override;

	void UpdateBoundingBox() override;

	FDynamicEmitterDataBase* GetDynamicData() const override;

	UParticleModuleTypeDataRibbon* RibbonTypeData = nullptr;
	int32 RibbonPayloadOffset = InvalidPayloadOffset;

protected:
	void PostSpawn(
	    FBaseParticle* Particle,
	    float InterpolationPercentage,
	    float SpawnTime) override;

private:
	bool HasValidRibbonSetup() const;

	FBaseParticle* GetParticleBySlot(int32 Slot);
	const FBaseParticle* GetParticleBySlot(int32 Slot) const;

	FRibbonParticlePayload ReadRibbonPayload(int32 Slot) const;
	void WriteRibbonPayload(
	    int32 Slot,
	    const FRibbonParticlePayload& Payload);

	int32 FindActiveIndex(int32 Slot) const;

	bool SpawnRibbonPoint(
	    const FVector& WorldPosition,
	    float SpawnAge,
	    double Distance);

	void ResetTrailTracking();

	bool TryBuildSourcePoint(FParticleStripPoint& OutPoint) const;

private:
	int32 HeadSlot = InvalidRibbonSlot;
	int32 TailSlot = InvalidRibbonSlot;

	bool bHasPreviousSource = false;
	FVector PreviousSource = FVector::ZeroVector;

	double DistanceRemainder = 0.0;
	double TotalSourceDistance = 0.0;

	// SpawnParticles → PostSpawn 사이에 전달할 생성 정보.
	FVector PendingSpawnPosition = FVector::ZeroVector;
	double PendingSpawnDistance = 0.0;
};

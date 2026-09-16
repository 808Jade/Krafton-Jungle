#pragma once

#include "BillboardComponent.h"

struct FParticle
{
	FVector Location;
	FVector Velocity;
	float LifeTime;

	float Age;
	float Scale;
	uint32 SubUVFrame;
	float FrameTimer;

	float Alpha;

	bool bAlive;
};

struct FSubUVConstants
{
	float CurrentFrame;
	float AtlasRowSize;
	float AtlasColSize;
	//float Padding = 0.0f;
	float Alpha;
};

// Todo: subuv
// 가장 단순한 SubUV 파티클입니다.
// Atlas 전체를 Col x Row로 나누고 CurrentFrame 하나만 변경합니다.
class UParticleSubUVComponent : public UBillboardComponent
{
	DECLARE_CLASS(UParticleSubUVComponent, UBillboardComponent)

	REFLECT_START(ClassName)
		PROPERTY(ColSize)
		PROPERTY(RowSize)
		PROPERTY(FrameRate)
	REFLECT_END()

public:
	UParticleSubUVComponent() = default;
	virtual ~UParticleSubUVComponent() = default;
	
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	virtual void SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue) override;

	void SetAtlasTexturePath(const FString& InPath);
	void SetSubUVSize(uint32 Cols, uint32 Rows);
	void SetFrameRate(float InFrameRate);

private:
	void RespawnParticle(FParticle& Particle);

	// Todo: Move to util class
	float GetRandomNumberBetween(float start, float end) const;
	float Lerp(float first, float second, float alpha) const;
	float GedSmoothStepedRatio(float start, float end, float value) const;

private:
	enum
	{
		MAX_PARTICLE_COMPONENTS_COUNT = 50,
	};

	static const float MAX_NORMALIZED_VALUE;
	static const float FADE_START;

	FString AtlasTexturePath;
	UTexture2D* AtlasTexture;

	uint32 ColSize;
	uint32 RowSize;
	float FrameTimer;
	float FrameRate;

	// Todo: Reserve particles
	TArray<FParticle> Particles;
};

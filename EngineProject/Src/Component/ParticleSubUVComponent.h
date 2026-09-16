#pragma once

#include "BillboardComponent.h"

struct FSubUVConstants
{
	float CurrentFrame;
	float AtlasRowSize;
	float AtlasColSize;
	float Padding = 0.0f;
};

// Todo: subuv
// 가장 단순한 SubUV 파티클입니다.
// Atlas 전체를 Col x Row로 나누고 CurrentFrame 하나만 변경합니다.
class UParticleSubUVComponent : public UBillboardComponent
{
	DECLARE_CLASS(UParticleSubUVComponent, UBillboardComponent)

	REFLECT_START(ClassName)
		REFLECT_END()

public:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	virtual void SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue) override;

	void SetAtlasTexturePath(const FString& InPath);
	void SetSubUVSize(uint32 Cols, uint32 Rows);
	void SetFrameRate(float InFrameRate);

private:
	FString AtlasTexturePath;
	UTexture2D* AtlasTexture;

	uint32 ColSize;
	uint32 RowSize;
	uint32 CurrentFrame;
	float FrameTimer;
	float FrameRate;
};

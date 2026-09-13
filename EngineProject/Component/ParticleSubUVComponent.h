#pragma once

#include "Component/PrimitiveComponent.h"

// Todo: subuv
// 가장 단순한 SubUV 파티클입니다.
// Atlas 전체를 Col x Row로 나누고 CurrentFrame 하나만 변경합니다.
class UParticleSubUVComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UParticleSubUVComponent, UPrimitiveComponent)

	REFLECT_START(ClassName)
		REFLECT_END()

public:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;
	virtual void SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue) override;

	// Todo: subuv
	void SetAtlasTexturePath(const FString& InPath) { AtlasTexturePath = InPath; }
	void SetSubUVSize(uint32 InCols, uint32 InRows);
	void SetFrameRate(float InFrameRate) { FrameRate = (InFrameRate > 0.0f) ? InFrameRate : 1.0f; }

private:
	// Todo: subuv
	FString AtlasTexturePath;
	uint32 NumCols;
	uint32 NumRows;
	uint32 CurrentFrame;
	float FrameTimer;
	float FrameRate;
	UTexture2D* AtlasTexture;
};

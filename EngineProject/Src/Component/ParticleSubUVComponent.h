#pragma once

#include "BillboardComponent.h"

class UParticleSubUVComponent : public UBillboardComponent
{
	DECLARE_CLASS(UParticleSubUVComponent, UPrimitiveComponent)

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

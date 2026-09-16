#pragma once

#include "PrimitiveComponent.h"

class UBillboardComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)

	REFLECT_START(ClassName)
		REFLECT_END()

public:
	UBillboardComponent();
	virtual ~UBillboardComponent() override;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	FMatrix GetBillboardWorldMatrix(const FMatrix& ViewMatrix);

	virtual void SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue) override;
private:

	
};
#pragma once

#include "PrimitiveComponent.h"

class UBillboardComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)

	REFLECT_START(ClassName)
		REFLECT_END()

public:
	UBillboardComponent() = default;
	virtual ~UBillboardComponent() = default;

	void GetWorldTransformedMatrix(FMatrix* OutWorldMatrix) const;
	//void SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue);

private:
};
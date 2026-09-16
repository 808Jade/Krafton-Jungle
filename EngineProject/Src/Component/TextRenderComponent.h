#pragma once

#include "BillboardComponent.h"

class UTextRenderComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UTextRenderComponent, UPrimitiveComponent)
public:
	UTextRenderComponent();
	virtual ~UTextRenderComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;
private:
	UFont Font;
};


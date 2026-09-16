#pragma once

#include "BillboardComponent.h"

class UTextRenderComponent : public UBillboardComponent
{
	DECLARE_CLASS(UTextRenderComponent, UBillboardComponent)
public:
	UTextRenderComponent();
	virtual ~UTextRenderComponent();
private:
	FString Text;
};


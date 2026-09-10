#pragma once

#include "PrimitiveComponent.h"

class USphereComponent :public UPrimitiveComponent
{
	DECLARE_CLASS(USphereComponent, UPrimitiveComponent)

	REFLECT_START(ClassName)
	REFLECT_END()

	USphereComponent() {};
	~USphereComponent() {};

public:
	virtual void BeginPlay() override;
private:

};
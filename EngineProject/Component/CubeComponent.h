#pragma once

#include "PrimitiveComponent.h"

class UCubeComponent :public UPrimitiveComponent
{
	DECLARE_CLASS(UCubeComponent, UPrimitiveComponent)

	REFLECT_START(ClassName)
		REFLECT_END()

		UCubeComponent() {};
	~UCubeComponent() {};

public:
	virtual void BeginPlay() override;

private:

};
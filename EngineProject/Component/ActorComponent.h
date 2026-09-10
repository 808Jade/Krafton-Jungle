#pragma once

#include "../ObjectSystem/Object.h"
#include "ObjectSystem/Class.h"

class UActorComponent : public UObject
{
	DECLARE_CLASS(UActorComponent, UObject)

	REFLECT_START(ClassName)
		REFLECT_END()
public:
	virtual void BeginPlay() {};
	virtual void TickComponent(float DeltaTime) {};
private:

};
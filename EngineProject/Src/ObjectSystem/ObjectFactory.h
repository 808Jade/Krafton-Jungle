#pragma once

#include "Object.h"

class FObjectFactory
{
public:
	static UObject* ConstructObject(UClass* Class);

	template <typename T>
	static T* ConstructObject()
	{
		return CastChecked<T>(ConstructObject(T::StaticClass()));
	}
private:

};
#include "EnginePCH.h"
#include "Object.h"

#include "Core/EngineStatics.h"
#include "ObjectSystem/Class.h"


TArray<UObject*> GUObjectArray;

UObject::UObject()
{
	ObjectUUID = FEngineStatics::GetUUID();
	InternalIndex = GUObjectArray.Num();
	GUObjectArray.Add(this);
	LOG(Info, "UUID : {}", ObjectUUID);
}

UObject::UObject(bool bRegister)
{
	bIsRegistered = bRegister;
}

UObject::~UObject()
{
	if (bIsRegistered)
	{
		int32 LastIndex = GUObjectArray.Num() - 1;
		if (InternalIndex != LastIndex)
		{
			UObject* MovedObject = GUObjectArray[LastIndex];
			GUObjectArray[InternalIndex] = MovedObject;
			MovedObject->InternalIndex = InternalIndex; // 옮겨간 오브젝트 인덱스 갱신
		}
		GUObjectArray.RemoveLast();
	}
}

UClass* UObject::StaticClass()
{
	static UClass c;
	static bool bIsInit = false;
	if (!bIsInit)
	{
		c.Name = "Object";
		c.Super = nullptr;
		c.Constructor = []() -> UObject*
			{
				return new UObject();
			};
		bIsInit = true;
	}
	return &c;
}

UClass* UObject::GetClass() const
{
	return StaticClass();
}

bool UObject::IsA(UClass* Class)
{
	return GetClass()->IsChildOf(Class);
}


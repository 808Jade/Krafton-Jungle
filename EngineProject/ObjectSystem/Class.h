#pragma once

#include "Property.h"
#include "Object.h"

using ClassConstructor = UObject * (*)();

class UClass : public UObject
{
public:
	UClass();

	FString Name;
	UClass* Super = nullptr;
	ClassConstructor Constructor = nullptr;

	TArray<FProperty> Properties;
	UObject* DefaultObject = nullptr;

	bool IsChildOf(UClass* BaseClass) const;
	UObject* GetDefaultObject();

	inline const TArray<FProperty>& GetProperties() const { return Properties; }

	template<class T>
	T* GetDefaultObject()
	{
		return CastChecked<T>(GetDefaultObject());
	}

	template <typename T>
	void AddProperty(const FString& InName, uint64 InOffset)
	{
		Properties.push_back({ InName, GetPropertyType<T>(), InOffset, sizeof(T) });
	}
};

//보류
inline void CopyProperties(UObject* Src, UObject* Dst, UClass* FromClass)
{
	for (UClass* c = FromClass; c; c = c->Super)
		for (const FProperty& p : c->Properties)
			memcpy((char*)Dst + p.Offset, (char*)Src + p.Offset, p.Size);
}
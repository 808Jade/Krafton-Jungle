#pragma once
#include "EnginePCH.h"
#include "../Container/StringView.h"

namespace UE::Core::Private
{
	struct FNameEntry
	{
		FString Name;
		FNameEntryId ComparisonId;
	};

	class FNamePool
	{
	public:
		FNamePool();

		FNameEntryId Store(FNameStringView Name);
		const FNameEntry& Resolve(FNameEntryId Id) const;

	private:
		static FString MakeComparisonKey(FNameStringView Name);

	private:
		TArray<FNameEntry> Entries;
		TMap<FString, FNameEntryId> DisplayLookup;
		TMap<FString, FNameEntryId> ComparisonLookup;
	};

	FNamePool& GetNamePool();
	void ParseNumberFromName(FString& Name, int32& OutInternalNumber);
}
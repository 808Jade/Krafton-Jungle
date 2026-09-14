#include "EnginePCH.h"
#include "Name.h"

namespace
{
	struct FNameEntry
	{
		FString Display;
		uint32 ComparisonIndex;
	};

	class FNameTable {
	public:
		FNameTable();

		FName Store(std::string_view Data);
		const FString& Resolve(uint32 Index) const { return Entries[Index]->Display; }
	private:
		uint32 AddEntry(std::string_view Data, uint32 ComparisonIndex);

		TArray<FNameEntry*> Entries;
		TMap<FString, uint32> DisplayLookup;
		TMap<FString, uint32> ComparisonLookup;
	};

	FNameTable::FNameTable()
	{
		Entries.Add(new FNameEntry{ "None", 0 });
		DisplayLookup.Add("None", 0);
		ComparisonLookup.Add("NONE", 0);
	}

	FName FNameTable::Store(std::string_view Data)
	{
		FString String = FString(Data.data(), Data.size());
		if (uint32* DisplayIndex = DisplayLookup.FindOrNull(String))
		{
			return FName(*DisplayIndex, Entries[*DisplayIndex]->ComparisonIndex);
		}

		//새로운 이름 테이블을 생성한다 ( Display 용 )
		FNameEntry* NewEntry = new FNameEntry();
		NewEntry->Display = FString(Data);

		FString UpperString = String;

		// 비교용 문자열
		for (auto& C : UpperString)
			C = std::toupper(C);

		uint32 NewIndex = Entries.Num();
		// 비교용 문자열은 존재하는지 확인
		uint32 ComparisonIndex = 0;
		if (uint32* Found = ComparisonLookup.FindOrNull(Data.data()))
		{
			ComparisonIndex = *Found;
		}
		else
		{
			ComparisonLookup.Add(UpperString, NewIndex);
			ComparisonIndex = NewIndex;
		}
		Entries.Add(new FNameEntry{ String, ComparisonIndex });
		DisplayLookup.Add(String, NewIndex);

		return FName(NewIndex, ComparisonIndex);
	}

	FNameTable& GetNameTable() { static FNameTable Table; return Table; }
}

FName::FName(char* pStr)
	:FName(FString(pStr))
{
}

FName::FName(FString str)
{
	*this = GetNameTable().Store(str);
}

FName::FName(uint32 InDisplayIndex, uint32 InComparisonIndex)
{
	DisplayIndex = InDisplayIndex;
	ComparisonIndex = InComparisonIndex;
}

int32 FName::Compare(const FName& Other) const
{
	return int32();
}

const FString& FName::ToString() const
{
	return GetNameTable().Resolve(DisplayIndex);
}


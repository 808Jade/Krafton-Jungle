#include "EnginePCH.h"
#include "NameTypes.h"
#include "Container/StringView.h"

namespace UE::Core::Private
{
	struct FNameEntry
	{
		FString Display;
		int32 ComparisonIndex = 0;
	};

	class FNamePool
	{
	public:
		FNamePool();


		FName Store(FStringView Name, int32 InInternalNumber = 0);

		const FString& Resolve(int32 Index) const { return Entries[Index].Display; }

	private:
		TArray<FNameEntry> Entries;

		// 실제 표시 문자열 검색
		TMap<FString, int32> DisplayLookup;

		// 대소문자를 무시한 비교용 문자열 검색
		TMap<FString, int32> ComparisonLookup;
	};

	FNamePool::FNamePool()
	{
		Entries.Add({ "None", 0 });

		DisplayLookup.Add("None", 0);
		ComparisonLookup.Add("NONE", 0);
	}

	static FString ToUpper(const FString& String)
	{
		FString UpperString = String;

		for (char& Char : UpperString)
		{
			Char = static_cast<char>(std::toupper(static_cast<unsigned char>(Char)));
		}

		return UpperString;
	}

	static void ParseNumberFromName( FString& Name, int32& OutInternalNumber)
	{
		// 1. '_' 위치를 특정.
		size_t UnderscorePosition = Name.find_last_of('_');

		if (UnderscorePosition == FString::npos)
			return;

		if (UnderscorePosition + 1 >= Name.length())
			return;

		// 2. '_' 뒤쪽 숫자를 파싱.

		OutInternalNumber = 0;

		FString NumberString = Name.substr(UnderscorePosition + 1);

		// 3. 숫자인지 확인.
		for (char& Char : NumberString)
		{
			if (!std::isdigit(static_cast<unsigned char>(Char)))
				return;
		}

		// 4. 숫자로 변환한 뒤 1 크게 저장.
		int32 ExternalNumber = std::stoi(NumberString);

		OutInternalNumber = ExternalNumber + 1;

		// 5. '_'앞 부분 이름 파싱.
		Name = Name.substr(0, UnderscorePosition);
	}

	FName FNamePool::Store( FStringView Name, int32 InInternalNumber)
	{
		if (Name.empty())
			return NAME_None;

		FString DisplayString(Name.data(), Name.size());

		// 1. 이미 존재하는 문자열인지? 대소문자 구분 O
		if(int32* DisplayIndex = DisplayLookup.Find(DisplayString))
		{
			int32 ComparisonIndex = Entries[*DisplayIndex].ComparisonIndex;
			return FName(ComparisonIndex, *DisplayIndex, InInternalNumber);
		}

		// 2. 문자열 새로 추가, 대문자로 비교할 때 같은 문자열이 있는지?
		// 있으면 해당 인덱스 재사용, 없으면 새 ComparisonIndex에 추가
		FString ComparisonString = ToUpper(DisplayString);
		int32 ComparisonIndex = 0;

		if (int32* Found = ComparisonLookup.Find(ComparisonString))
		{
			ComparisonIndex = *Found;
		}
		else
		{
			ComparisonIndex = Entries.Num();
			ComparisonLookup.Add(ComparisonString, ComparisonIndex );
		}

		// 3. 다음 엔트리에 저장
		int32 DisplayIndex = Entries.Num();

		Entries.Add( { DisplayString, ComparisonIndex });
		DisplayLookup.Add(DisplayString, DisplayIndex);


		return FName(ComparisonIndex, DisplayIndex, InInternalNumber);
	}

	FNamePool& GetNamePool()
	{
		static FNamePool NamePool;
		return NamePool;
	}
}

FName::FName(const char* Name) : FName(FString(Name))
{
}

FName::FName(const FString& Name)
{
	if (Name.empty())
		return;

	FString PlainName = Name;

	int32 InternalNumber = 0;

	UE::Core::Private::ParseNumberFromName(PlainName, InternalNumber);

	*this = UE::Core::Private::GetNamePool().Store(PlainName, InternalNumber);
}


FName::FName(FName Other, int32 InNumber) 
	: ComparisonIndex(Other.ComparisonIndex),
	DisplayIndex(Other.DisplayIndex),
	Number(static_cast<uint32>(InNumber))
{
}

FName::FName(int32 InComparisonIndex, int32 InDisplayIndex, int32 InNumber)
	: ComparisonIndex(InComparisonIndex),
	DisplayIndex(InDisplayIndex),
	Number(static_cast<uint32>(InNumber))
{
}

int32 FName::Compare(const FName& Other) const
{
	if (ComparisonIndex == Other.ComparisonIndex)
	{
		if (Number < Other.Number)
			return -1;

		if (Number > Other.Number)
			return 1;

		return 0;
	}

	FString ThisString =
		UE::Core::Private::ToUpper(GetPlainNameString());

	FString OtherString =
		UE::Core::Private::ToUpper(Other.GetPlainNameString());

	if (ThisString < OtherString)
		return -1;

	if (ThisString > OtherString)
		return 1;

	return 0;
}

FString FName::ToString() const
{
	FString Result = GetPlainNameString();

	// 내부 Number 0 = 번호 없음
	if (Number > 0)
	{
		// 내부 1 → 화면 _0
		Result += "_" + std::to_string(Number - 1);
	}

	return Result;
}

FString FName::GetPlainNameString() const
{
	return UE::Core::Private::GetNamePool().Resolve( DisplayIndex );
}

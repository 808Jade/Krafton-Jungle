#pragma once

struct FName
{
	FName() = default;
	FName(char* pStr);
	FName(FString str);
	FName(uint32 DisplayIndex, uint32 ComparisonIndex);

	inline int32 Compare(const FName& Other) const;
	bool operator==(const FName& Other) const { return ComparisonIndex == Other.ComparisonIndex; };

	const FString& ToString() const;

	int32 DisplayIndex = 0;
	int32 ComparisonIndex = 0;
};
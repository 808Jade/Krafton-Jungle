#pragma once

#include "Containers/Array.h"
#include "Core/EngineString.h"
#include "Serialization/StructuredArchiveSlots.h"

// 키와 키 사이를 잇는 방식. (언리얼의 EInterpCurveMode와 같은 의미)
enum class EInterpCurveMode : uint8
{
	Linear,				// 직선
	Constant,			// 다음 키까지 값을 유지하다가 계단처럼 바뀐다
	CurveAuto,			// 앞뒤 키로 탄젠트를 자동 계산한 부드러운 곡선
	CurveAutoClamped,	// 자동 곡선이지만 극값(산·골)에서는 탄젠트를 0으로 눌러 넘침을 막는다
	CurveUser,			// 사용자가 정한 탄젠트. 들어오는·나가는 탄젠트가 같다
	CurveBreak,			// 사용자가 정한 탄젠트. 들어오는·나가는 탄젠트를 따로 둔다 (꺾임)
};

// 커브의 키 하나. InVal(가로축, 파티클은 0~1 수명 비율)에서 OutVal 값을 가진다.
struct FInterpCurvePointFloat
{
	float InVal = 0.0f;
	float OutVal = 0.0f;
	// 키로 들어오는·나가는 기울기 (OutVal / InVal 단위)
	float ArriveTangent = 0.0f;
	float LeaveTangent = 0.0f;
	EInterpCurveMode InterpMode = EInterpCurveMode::CurveAuto;

	bool IsCurveKey() const
	{
		return InterpMode != EInterpCurveMode::Linear && InterpMode != EInterpCurveMode::Constant;
	}
};

// 키 목록으로 정의하는 float 커브. (언리얼의 FInterpCurveFloat에 해당)
// 키는 항상 InVal 오름차순으로 유지한다. 키를 바꾼 뒤에는 AutoSetTangents로 자동 탄젠트를 다시 계산한다.
struct FInterpCurveFloat
{
	TArray<FInterpCurvePointFloat> Points;

	// 키가 없으면 Default, 범위 밖이면 양 끝 키 값을 돌려준다.
	float Eval(float InVal, float Default = 0.0f) const;

	// 정렬된 위치에 키를 넣고 그 인덱스를 돌려준다.
	int32 AddPoint(float InVal, float OutVal, EInterpCurveMode Mode = EInterpCurveMode::CurveAuto);
	void RemovePoint(int32 Index);
	// 키의 가로 위치를 바꾸고 다시 정렬한다. 바뀐 인덱스를 돌려준다.
	int32 MovePoint(int32 Index, float NewInVal);
	// CurveAuto·CurveAutoClamped·Linear 키의 탄젠트를 앞뒤 키로 다시 계산한다.
	void AutoSetTangents();

	// 키가 하나라도 있으면 true. 가로·세로 범위를 돌려준다. (곡선이 키 사이에서 넘치는 부분까지 포함한다)
	bool GetInRange(float& OutMin, float& OutMax) const;
	bool GetOutRange(float& OutMin, float& OutMax) const;

	// { "Points": [ { In, Out, Arrive, Leave, Mode } ... ] }
	void Serialize(FStructuredArchiveSlot Slot);

	// From에서 To로 가는 두 키(0, 1) 직선 커브
	static FInterpCurveFloat MakeLinear(float From, float To);
};

// 커브 에디터가 모듈의 커브를 찾아 편집할 때 쓰는 참조. 모듈이 GetCurves로 내놓는다.
struct FParticleCurveRef
{
	// 커브 에디터 목록에 보이는 이름 (예: "Size Scale X")
	FString Name;
	// 참조(집약): 모듈이 소유하는 커브
	FInterpCurveFloat* Curve = nullptr;
	// 그래프 선 색 (RGBA 0~1)
	float Color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	// 키 시간 연결 그룹. 같은 모듈에서 값이 같은(0 이상) 커브끼리는 키 시간을 함께 쓴다.
	// (언리얼의 Vector 분포처럼 R/G/B·X/Y/Z 중 하나의 키를 옮기거나 추가·삭제하면 나머지도 같은 시간에서 따라간다) -1이면 따로 논다.
	int32 LinkGroup = -1;
};

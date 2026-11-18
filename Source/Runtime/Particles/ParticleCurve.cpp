#include "EnginePCH.h"
#include "Particles/ParticleCurve.h"
#include "Serialization/StructuredArchive.h"

#include <algorithm>
#include <cmath>

namespace
{
	// 3차 에르미트 보간. P0·P1은 양 끝 값, T0·T1은 구간 길이를 곱한 탄젠트, A는 0~1
	float CubicInterp(float P0, float T0, float P1, float T1, float A)
	{
		const float A2 = A * A;
		const float A3 = A2 * A;
		return (2 * A3 - 3 * A2 + 1) * P0 + (A3 - 2 * A2 + A) * T0 + (A3 - A2) * T1 + (-2 * A3 + 3 * A2) * P1;
	}
}

float FInterpCurveFloat::Eval(const float InVal, const float Default) const
{
	const int32 Num = Points.Num();
	if (Num == 0)
		return Default;
	if (Num == 1 || InVal <= Points[0].InVal)
		return Points[0].OutVal;
	if (InVal >= Points[Num - 1].InVal)
		return Points[Num - 1].OutVal;

	// 키 수가 적어(보통 2~8개) 선형 탐색으로 충분하다.
	int32 Index = 0;
	while (Index + 1 < Num && Points[Index + 1].InVal <= InVal)
		++Index;

	const FInterpCurvePointFloat& P0 = Points[Index];
	const FInterpCurvePointFloat& P1 = Points[Index + 1];
	const float Diff = P1.InVal - P0.InVal;
	if (Diff <= 0.0f || P0.InterpMode == EInterpCurveMode::Constant)
		return P0.OutVal;

	const float Alpha = (InVal - P0.InVal) / Diff;
	if (P0.InterpMode == EInterpCurveMode::Linear)
		return P0.OutVal + (P1.OutVal - P0.OutVal) * Alpha;
	return CubicInterp(P0.OutVal, P0.LeaveTangent * Diff, P1.OutVal, P1.ArriveTangent * Diff, Alpha);
}

int32 FInterpCurveFloat::AddPoint(const float InVal, const float OutVal, const EInterpCurveMode Mode)
{
	int32 Index = 0;
	while (Index < Points.Num() && Points[Index].InVal < InVal)
		++Index;

	FInterpCurvePointFloat Point;
	Point.InVal = InVal;
	Point.OutVal = OutVal;
	Point.InterpMode = Mode;
	Points.Insert(Point, static_cast<uint32>(Index));
	AutoSetTangents();
	return Index;
}

void FInterpCurveFloat::RemovePoint(const int32 Index)
{
	if (!Points.IsValidIndex(Index))
		return;
	Points.RemoveAt(static_cast<uint32>(Index));
	AutoSetTangents();
}

int32 FInterpCurveFloat::MovePoint(int32 Index, const float NewInVal)
{
	if (!Points.IsValidIndex(Index))
		return Index;

	Points[Index].InVal = NewInVal;
	// 이웃을 넘어가면 자리를 바꿔 오름차순을 유지한다.
	while (Index > 0 && Points[Index - 1].InVal > Points[Index].InVal)
	{
		std::swap(Points[Index - 1], Points[Index]);
		--Index;
	}
	while (Index + 1 < Points.Num() && Points[Index + 1].InVal < Points[Index].InVal)
	{
		std::swap(Points[Index + 1], Points[Index]);
		++Index;
	}
	AutoSetTangents();
	return Index;
}

void FInterpCurveFloat::AutoSetTangents()
{
	const int32 Num = Points.Num();
	for (int32 i = 0; i < Num; ++i)
	{
		FInterpCurvePointFloat& P = Points[i];
		if (P.InterpMode == EInterpCurveMode::CurveUser || P.InterpMode == EInterpCurveMode::CurveBreak)
			continue;

		float Tangent = 0.0f;
		if (P.InterpMode == EInterpCurveMode::Linear || P.InterpMode == EInterpCurveMode::Constant)
		{
			// 그래프에 보여 줄 기울기만 둔다. 보간에는 쓰지 않는다.
			if (i + 1 < Num && Points[i + 1].InVal > P.InVal)
				Tangent = (Points[i + 1].OutVal - P.OutVal) / (Points[i + 1].InVal - P.InVal);
		}
		else if (i > 0 && i + 1 < Num)
		{
			const FInterpCurvePointFloat& Prev = Points[i - 1];
			const FInterpCurvePointFloat& Next = Points[i + 1];
			// 극값(앞뒤보다 모두 크거나 모두 작음)에서 자동/제한은 평평하게 눌러 곡선이 키 값을 넘지 않게 한다.
			const bool bExtremum = (P.OutVal >= Prev.OutVal && P.OutVal >= Next.OutVal)
				|| (P.OutVal <= Prev.OutVal && P.OutVal <= Next.OutVal);
			if (!(P.InterpMode == EInterpCurveMode::CurveAutoClamped && bExtremum) && Next.InVal > Prev.InVal)
				Tangent = (Next.OutVal - Prev.OutVal) / (Next.InVal - Prev.InVal);
		}
		// 양 끝 키는 0 (평평하게 시작·끝)
		P.ArriveTangent = Tangent;
		P.LeaveTangent = Tangent;
	}
}

bool FInterpCurveFloat::GetInRange(float& OutMin, float& OutMax) const
{
	if (Points.Num() == 0)
		return false;
	OutMin = Points[0].InVal;
	OutMax = Points[Points.Num() - 1].InVal;
	return true;
}

bool FInterpCurveFloat::GetOutRange(float& OutMin, float& OutMax) const
{
	if (Points.Num() == 0)
		return false;
	OutMin = OutMax = Points[0].OutVal;
	// 곡선 구간은 키 사이에서 키 값을 넘칠 수 있으므로 샘플링해서 범위를 잡는다.
	float InMin, InMax;
	GetInRange(InMin, InMax);
	constexpr int32 Samples = 64;
	for (int32 s = 0; s <= Samples; ++s)
	{
		const float V = Eval(InMin + (InMax - InMin) * static_cast<float>(s) / Samples);
		OutMin = std::min(OutMin, V);
		OutMax = std::max(OutMax, V);
	}
	for (const FInterpCurvePointFloat& P : Points)
	{
		OutMin = std::min(OutMin, P.OutVal);
		OutMax = std::max(OutMax, P.OutVal);
	}
	return true;
}

void FInterpCurveFloat::Serialize(FStructuredArchiveSlot Slot)
{
	FStructuredArchiveRecord Record = Slot.EnterRecord();
	const bool bLoading = Record.GetUnderlyingArchive().IsLoading();

	int32 Num = Points.Num();
	FStructuredArchiveArray Array = Record.EnterArray(FArchiveFieldName("Points"), Num);
	if (bLoading)
		Points.SetNum(Num);

	for (int32 Index = 0; Index < Num; ++Index)
	{
		FInterpCurvePointFloat& P = Points[Index];
		int32 Mode = static_cast<int32>(P.InterpMode);
		FStructuredArchiveRecord PointRecord = Array.EnterElement().EnterRecord();
		PointRecord << SA_VALUE("In", P.InVal) << SA_VALUE("Out", P.OutVal)
			<< SA_VALUE("Arrive", P.ArriveTangent) << SA_VALUE("Leave", P.LeaveTangent) << SA_VALUE("Mode", Mode);
		if (bLoading)
			P.InterpMode = static_cast<EInterpCurveMode>(std::clamp(Mode, 0, static_cast<int32>(EInterpCurveMode::CurveBreak)));
	}
}

FInterpCurveFloat FInterpCurveFloat::MakeLinear(const float From, const float To)
{
	// 예전 Start→End 선형 보간과 똑같이 움직이도록 직선 키로 만든다. (자동 곡선이면 양 끝이 평평한 S자가 된다)
	FInterpCurveFloat Curve;
	Curve.AddPoint(0.0f, From, EInterpCurveMode::Linear);
	Curve.AddPoint(1.0f, To, EInterpCurveMode::Linear);
	return Curve;
}

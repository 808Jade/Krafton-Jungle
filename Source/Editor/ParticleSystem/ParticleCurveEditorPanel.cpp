#include "EnginePCH.h"
#include "Editor/ParticleSystem/ParticleCurveEditorPanel.h"
#include "Particles/Modules/ParticleModuleBeamSource.h"
#include "Particles/Modules/ParticleModuleBeamTarget.h"
#include "Editor/ParticleSystem/ParticleEditorContext.h"
#include "Editor/HitoriEd/EditorResourcePaths.h"
#include "Asset/AssetManager.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Render/Texture2D.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace
{
	constexpr float TrackListWidth = 210.0f;
	constexpr float ToolIconSize = 32.0f;
	constexpr float ToolButtonWidth = 54.0f;
	constexpr float KeyHalfSize = 4.0f;
	constexpr float KeyHitRadius = 7.0f;
	constexpr float TangentHandleLength = 45.0f;
	constexpr float CurvePickDistance = 10.0f;

	const ImU32 GraphBackground = IM_COL32(58, 58, 58, 255);
	const ImU32 GridLine = IM_COL32(85, 85, 85, 255);
	const ImU32 GridAxis = IM_COL32(140, 140, 140, 255);
	const ImU32 GridText = IM_COL32(200, 200, 200, 255);
	const ImU32 ActiveButton = IM_COL32(218, 130, 10, 255);
	const ImU32 SelectedKeyColor = IM_COL32(255, 255, 255, 255);
	const ImU32 TangentColor = IM_COL32(220, 220, 220, 200);

	ImU32 ToImColor(const float Color[4], float Alpha = 1.0f)
	{
		return ImGui::ColorConvertFloat4ToU32(ImVec4(Color[0], Color[1], Color[2], Color[3] * Alpha));
	}

	// 화면에 8~12개 정도의 눈금이 오도록 1·2·5 × 10^n 간격을 고른다.
	float NiceGridStep(float Range, float Pixels, float TargetPixels)
	{
		if (Range <= 0.0f || Pixels <= 0.0f)
			return 1.0f;
		const float Raw = Range * TargetPixels / Pixels;
		const float Pow = std::pow(10.0f, std::floor(std::log10(Raw)));
		const float Norm = Raw / Pow;
		const float Nice = Norm < 1.5f ? 1.0f : Norm < 3.5f ? 2.0f : Norm < 7.5f ? 5.0f : 10.0f;
		return Nice * Pow;
	}

	// "UParticleModuleSize" → "Size"
	FString GetModuleShortName(const UParticleModule* Module)
	{
		FString Name = Module->GetClass()->GetName();
		for (const char* Prefix : { "UParticleModule", "ParticleModule" })
		{
			const FString P(Prefix);
			if (Name.rfind(P, 0) == 0 && Name.size() > P.size())
			{
				Name.erase(0, P.size());
				break;
			}
		}
		return Name;
	}

	const char* GetInterpModeName(EInterpCurveMode Mode)
	{
		switch (Mode)
		{
		case EInterpCurveMode::Linear:           return "Linear";
		case EInterpCurveMode::Constant:         return "Constant";
		case EInterpCurveMode::CurveAuto:        return "Auto";
		case EInterpCurveMode::CurveAutoClamped: return "Auto/Clamped";
		case EInterpCurveMode::CurveUser:        return "User";
		case EInterpCurveMode::CurveBreak:       return "Break";
		}
		return "?";
	}
}

bool FParticleCurveEditorPanel::Init()
{
	LoadIcons();
	FCurveTab Default;
	Default.Name = "Default";
	Tabs.Add(Default);
	return true;
}

void FParticleCurveEditorPanel::LoadIcons()
{
	const auto Load = [](const char* Name)
	{
		return UAssetManager::Get().LoadTexture(FString(EditorResourcePaths::CurveEditorIconFolder) + Name + ".png");
	};
	Icons.FitHorizontal = Load("FitHorizontal");
	Icons.FitVertical = Load("FitVertical");
	Icons.FitSelected = Load("FitSelected");
	Icons.Pan = Load("Pan");
	Icons.Zoom = Load("Zoom");
	Icons.Auto = Load("Auto");
	Icons.AutoClamped = Load("AutoClamped");
	Icons.User = Load("User");
	Icons.Break = Load("Break");
	Icons.Linear = Load("Linear");
	Icons.Constant = Load("Constant");
	Icons.Flatten = Load("Flatten");
	Icons.Straighten = Load("Straighten");
	Icons.ShowAll = Load("ShowAll");
	Icons.CreateTab = Load("CreateTab");
	Icons.DeleteTab = Load("DeleteTab");
}

// ---------------------------------------------------------------------------------------------
// 트랙 관리
// ---------------------------------------------------------------------------------------------

bool FParticleCurveEditorPanel::IsModuleInAsset(const UParticleModule* Module) const
{
	const UParticleSystem* System = Context.GetTemplate();
	if (!System || !Module)
		return false;
	for (const UParticleEmitter* Emitter : System->Emitters)
	{
		if (!Emitter)
			continue;
		for (const UParticleLODLevel* LOD : Emitter->LODLevels)
		{
			if (!LOD)
				continue;
			for (const UParticleModule* Candidate : LOD->Modules)
			{
				if (Candidate == Module)
					return true;
			}
		}
	}
	return false;
}

void FParticleCurveEditorPanel::SyncTracks()
{
	// 1. 지워진 모듈(이미터 삭제·LOD 재생성 등)의 트랙을 모든 탭에서 뺀다. 선택도 비운다.
	bool bRemoved = false;
	for (FCurveTab& Tab : Tabs)
	{
		for (int32 i = Tab.Tracks.Num() - 1; i >= 0; --i)
		{
			if (!IsModuleInAsset(Tab.Tracks[i].Module))
			{
				Tab.Tracks.RemoveAt(static_cast<uint32>(i));
				bRemoved = true;
			}
		}
	}
	if (bRemoved)
		SelectedKeys.Reset();

	// 2. 이미터 패널에서 새로 고른 모듈에 커브가 있으면 현재 탭에 올린다. (Cascade에서 모듈의 커브 버튼을 누른 것과 같다)
	UParticleModule* Selected = Context.SelectedModule;
	if (!Selected || !IsModuleInAsset(Selected))
	{
		LastSelectedModule = nullptr;
		LastSelectedCurveCount = -1;
		return;
	}

	TArray<FParticleCurveRef> Curves;
	Selected->GetCurves(Curves);
	// Changing a distribution can expose curves without changing the selection.
	if (Selected == LastSelectedModule && Curves.Num() == LastSelectedCurveCount) return;
	LastSelectedModule = Selected;
	LastSelectedCurveCount = Curves.Num();
	if (Curves.IsEmpty())
		return;
	for (const FTrack& Track : CurrentTab().Tracks)
	{
		if (Track.Module == Selected)
			return;
	}

	FTrack Track;
	Track.Module = Selected;
	const int32 EmitterIndex = Context.SelectedEmitterIndex;
	Track.Label = (EmitterIndex >= 0 ? "E" + std::to_string(EmitterIndex) + ": " : FString()) + GetModuleShortName(Selected);
	Track.SubVisible.SetNum(Curves.Num());
	for (int32 i = 0; i < Curves.Num(); ++i)
		Track.SubVisible[i] = 1;
	CurrentTab().Tracks.Add(Track);
}

const FParticleCurveRef* FParticleCurveEditorPanel::GetCurveRef(const int32 TrackIndex, const int32 Sub)
{
	// 모듈이 매번 새 배열로 내놓으므로 프레임 안에서만 쓰는 임시 보관소에 담는다.
	static TArray<FParticleCurveRef> Scratch;
	FCurveTab& Tab = CurrentTab();
	if (!Tab.Tracks.IsValidIndex(TrackIndex))
		return nullptr;
	Scratch.Reset();
	Tab.Tracks[TrackIndex].Module->GetCurves(Scratch);
	return Scratch.IsValidIndex(Sub) ? &Scratch[Sub] : nullptr;
}

FInterpCurveFloat* FParticleCurveEditorPanel::GetCurve(const FKeyRef& Ref)
{
	const FParticleCurveRef* CurveRef = GetCurveRef(Ref.Track, Ref.Sub);
	return CurveRef ? CurveRef->Curve : nullptr;
}

void FParticleCurveEditorPanel::MarkEdited()
{
	Context.MarkDirty();
}

// ---------------------------------------------------------------------------------------------
// 좌표 변환
// ---------------------------------------------------------------------------------------------

ImVec2 FParticleCurveEditorPanel::ToScreen(const float X, const float Y) const
{
	const float W = GraphMax.x - GraphMin.x;
	const float H = GraphMax.y - GraphMin.y;
	return ImVec2(GraphMin.x + (X - ViewMinX) / (ViewMaxX - ViewMinX) * W,
		GraphMax.y - (Y - ViewMinY) / (ViewMaxY - ViewMinY) * H);
}

float FParticleCurveEditorPanel::ToCurveX(const float ScreenX) const
{
	return ViewMinX + (ScreenX - GraphMin.x) / std::max(1.0f, GraphMax.x - GraphMin.x) * (ViewMaxX - ViewMinX);
}

float FParticleCurveEditorPanel::ToCurveY(const float ScreenY) const
{
	return ViewMinY + (GraphMax.y - ScreenY) / std::max(1.0f, GraphMax.y - GraphMin.y) * (ViewMaxY - ViewMinY);
}

ImVec2 FParticleCurveEditorPanel::TangentHandleOffset(const float Tangent, const bool bLeave) const
{
	// 커브 공간의 방향 (1, Tangent)을 화면 축척으로 바꾼 뒤 일정한 화면 길이로 맞춘다.
	const float ScaleX = (GraphMax.x - GraphMin.x) / (ViewMaxX - ViewMinX);
	const float ScaleY = (GraphMax.y - GraphMin.y) / (ViewMaxY - ViewMinY);
	ImVec2 Dir(ScaleX, -Tangent * ScaleY);
	const float Len = std::sqrt(Dir.x * Dir.x + Dir.y * Dir.y);
	if (Len < 1e-6f)
		return ImVec2(0, 0);
	const float Sign = bLeave ? 1.0f : -1.0f;
	return ImVec2(Dir.x / Len * TangentHandleLength * Sign, Dir.y / Len * TangentHandleLength * Sign);
}

bool FParticleCurveEditorPanel::ShouldShowTangents(const FKeyRef& Ref, const FInterpCurvePointFloat& Point) const
{
	if (!Point.IsCurveKey())
		return false;
	return bShowAllTangents || SelectedKeys.Find(Ref) != INDEX_NONE;
}

// ---------------------------------------------------------------------------------------------
// 편집 동작
// ---------------------------------------------------------------------------------------------

void FParticleCurveEditorPanel::FitView(const bool bHorizontal, const bool bVertical, const bool bSelectedOnly)
{
	float MinX = FLT_MAX, MaxX = -FLT_MAX, MinY = FLT_MAX, MaxY = -FLT_MAX;
	const auto Include = [&](float X, float Y)
	{
		MinX = std::min(MinX, X); MaxX = std::max(MaxX, X);
		MinY = std::min(MinY, Y); MaxY = std::max(MaxY, Y);
	};

	if (bSelectedOnly && !SelectedKeys.IsEmpty())
	{
		for (const FKeyRef& Ref : SelectedKeys)
		{
			const FInterpCurveFloat* Curve = GetCurve(Ref);
			if (Curve && Curve->Points.IsValidIndex(Ref.Key))
				Include(Curve->Points[Ref.Key].InVal, Curve->Points[Ref.Key].OutVal);
		}
	}
	else
	{
		FCurveTab& Tab = CurrentTab();
		for (int32 t = 0; t < Tab.Tracks.Num(); ++t)
		{
			TArray<FParticleCurveRef> Curves;
			Tab.Tracks[t].Module->GetCurves(Curves);
			for (int32 s = 0; s < Curves.Num(); ++s)
			{
				if (!Tab.Tracks[t].IsSubVisible(s))
					continue;
				float InMin, InMax, OutMin, OutMax;
				if (Curves[s].Curve->GetInRange(InMin, InMax) && Curves[s].Curve->GetOutRange(OutMin, OutMax))
				{
					Include(InMin, OutMin);
					Include(InMax, OutMax);
				}
			}
		}
	}

	if (MinX > MaxX)
	{
		MinX = 0.0f; MaxX = 1.0f; MinY = 0.0f; MaxY = 1.0f;
	}
	// 한 점이거나 평평하면 보기 좋은 최소 폭을 준다. 가장자리에 키가 붙지 않게 여백을 둔다.
	const auto Expand = [](float& Lo, float& Hi)
	{
		float Span = Hi - Lo;
		if (Span < 0.1f)
		{
			const float Mid = (Lo + Hi) * 0.5f;
			Lo = Mid - 0.5f; Hi = Mid + 0.5f; Span = 1.0f;
		}
		Lo -= Span * 0.08f;
		Hi += Span * 0.08f;
	};
	Expand(MinX, MaxX);
	Expand(MinY, MaxY);
	if (bHorizontal) { ViewMinX = MinX; ViewMaxX = MaxX; }
	if (bVertical) { ViewMinY = MinY; ViewMaxY = MaxY; }
}

void FParticleCurveEditorPanel::SetSelectedInterpMode(const EInterpCurveMode Mode)
{
	for (const FKeyRef& Ref : SelectedKeys)
	{
		FInterpCurveFloat* Curve = GetCurve(Ref);
		if (!Curve || !Curve->Points.IsValidIndex(Ref.Key))
			continue;
		// 사용자·꺾임으로 바꿀 때는 지금 보이는 자동 탄젠트를 그대로 사용자 값으로 넘겨받는다.
		Curve->Points[Ref.Key].InterpMode = Mode;
		Curve->AutoSetTangents();
	}
	MarkEdited();
}

void FParticleCurveEditorPanel::FlattenSelectedTangents()
{
	for (const FKeyRef& Ref : SelectedKeys)
	{
		FInterpCurveFloat* Curve = GetCurve(Ref);
		if (!Curve || !Curve->Points.IsValidIndex(Ref.Key))
			continue;
		FInterpCurvePointFloat& P = Curve->Points[Ref.Key];
		if (P.InterpMode != EInterpCurveMode::CurveBreak)
			P.InterpMode = EInterpCurveMode::CurveUser;
		P.ArriveTangent = P.LeaveTangent = 0.0f;
	}
	MarkEdited();
}

void FParticleCurveEditorPanel::StraightenSelectedTangents()
{
	for (const FKeyRef& Ref : SelectedKeys)
	{
		FInterpCurveFloat* Curve = GetCurve(Ref);
		if (!Curve || !Curve->Points.IsValidIndex(Ref.Key))
			continue;
		FInterpCurvePointFloat& P = Curve->Points[Ref.Key];
		const float Average = (P.ArriveTangent + P.LeaveTangent) * 0.5f;
		P.InterpMode = EInterpCurveMode::CurveUser;
		P.ArriveTangent = P.LeaveTangent = Average;
	}
	MarkEdited();
}

void FParticleCurveEditorPanel::DeleteSelectedKeys()
{
	// 연결된 커브(R/G/B 등)의 같은 시간 키도 함께 지운다.
	TArray<FKeyRef> Targets = SelectedKeys;
	for (const FKeyRef& Ref : SelectedKeys)
		AppendLinkedKeys(Ref, Targets);
	RemoveKeys(Targets);
	SelectedKeys.Reset();
	MarkEdited();
}

void FParticleCurveEditorPanel::RemoveKeys(TArray<FKeyRef> Keys)
{
	// 같은 커브에서 여러 키를 지울 때 인덱스가 밀리지 않게 큰 인덱스부터 지운다.
	std::sort(Keys.begin(), Keys.end(), [](const FKeyRef& A, const FKeyRef& B) { return A.Key > B.Key; });
	for (const FKeyRef& Ref : Keys)
	{
		FInterpCurveFloat* Curve = GetCurve(Ref);
		// 마지막 키는 남긴다. 키가 없으면 커브가 기본값(배율 1)이 되어 편집할 손잡이가 사라진다.
		if (Curve && Curve->Points.IsValidIndex(Ref.Key) && Curve->Points.Num() > 1)
			Curve->RemovePoint(Ref.Key);
	}
}

// ---------------------------------------------------------------------------------------------
// 키 시간 연결 (언리얼 Vector 분포처럼 R/G/B·X/Y/Z 키의 시간을 함께 움직인다)
// ---------------------------------------------------------------------------------------------

namespace
{
	// 이 차이 안의 시간이면 같은 키로 본다.
	constexpr float LinkTimeTolerance = 1e-4f;
}

void FParticleCurveEditorPanel::AppendLinkedKeys(const FKeyRef& Ref, TArray<FKeyRef>& OutKeys)
{
	FCurveTab& Tab = CurrentTab();
	if (!Tab.Tracks.IsValidIndex(Ref.Track))
		return;
	TArray<FParticleCurveRef> Curves;
	Tab.Tracks[Ref.Track].Module->GetCurves(Curves);
	if (!Curves.IsValidIndex(Ref.Sub))
		return;
	const FParticleCurveRef& Source = Curves[Ref.Sub];
	if (Source.LinkGroup < 0 || !Source.Curve || !Source.Curve->Points.IsValidIndex(Ref.Key))
		return;

	const float Time = Source.Curve->Points[Ref.Key].InVal;
	for (int32 s = 0; s < Curves.Num(); ++s)
	{
		if (s == Ref.Sub || Curves[s].LinkGroup != Source.LinkGroup || !Curves[s].Curve)
			continue;
		const TArray<FInterpCurvePointFloat>& Points = Curves[s].Curve->Points;
		for (int32 k = 0; k < Points.Num(); ++k)
		{
			if (std::abs(Points[k].InVal - Time) <= LinkTimeTolerance)
			{
				const FKeyRef Linked{ Ref.Track, s, k };
				if (OutKeys.Find(Linked) == INDEX_NONE)
					OutKeys.Add(Linked);
				break;
			}
		}
	}
}

void FParticleCurveEditorPanel::AddLinkedPoints(const int32 Track, const int32 Sub, const float Time)
{
	FCurveTab& Tab = CurrentTab();
	if (!Tab.Tracks.IsValidIndex(Track))
		return;
	TArray<FParticleCurveRef> Curves;
	Tab.Tracks[Track].Module->GetCurves(Curves);
	if (!Curves.IsValidIndex(Sub) || Curves[Sub].LinkGroup < 0)
		return;

	for (int32 s = 0; s < Curves.Num(); ++s)
	{
		if (s == Sub || Curves[s].LinkGroup != Curves[Sub].LinkGroup || !Curves[s].Curve)
			continue;
		FInterpCurveFloat& Curve = *Curves[s].Curve;
		bool bExists = false;
		for (const FInterpCurvePointFloat& P : Curve.Points)
			bExists |= std::abs(P.InVal - Time) <= LinkTimeTolerance;
		if (!bExists)
		{
			// 곡선 모양이 바뀌지 않게 지금 그 시간의 값으로 키를 넣는다.
			Curve.AddPoint(Time, Curve.Eval(Time, 1.0f));
			Curve.AutoSetTangents();
		}
	}
}

// ---------------------------------------------------------------------------------------------
// 트랙 구분 (Cascade처럼 고른 트랙만 편집 대상으로 강조한다)
// ---------------------------------------------------------------------------------------------

ImU32 FParticleCurveEditorPanel::GetTrackColor(const int32 TrackIndex, const float Alpha)
{
	// 커브 선 색(R/G/B 채널색)과 겹치지 않는 색 위주로 고른다.
	static const ImVec4 Palette[] = {
		ImVec4(1.00f, 0.62f, 0.15f, 1.0f),	// 주황
		ImVec4(0.30f, 0.85f, 0.95f, 1.0f),	// 하늘
		ImVec4(0.95f, 0.45f, 0.85f, 1.0f),	// 분홍
		ImVec4(0.95f, 0.90f, 0.30f, 1.0f),	// 노랑
		ImVec4(0.65f, 0.55f, 1.00f, 1.0f),	// 보라
		ImVec4(0.55f, 0.95f, 0.55f, 1.0f),	// 연두
	};
	ImVec4 C = Palette[static_cast<uint32>(TrackIndex) % (sizeof(Palette) / sizeof(Palette[0]))];
	C.w = Alpha;
	return ImGui::ColorConvertFloat4ToU32(C);
}

void FParticleCurveEditorPanel::UpdateActiveTrack()
{
	const FCurveTab& Tab = CurrentTab();
	int32 NewActive = -1;
	for (int32 t = 0; t < Tab.Tracks.Num(); ++t)
	{
		if (Tab.Tracks[t].Module == Context.SelectedModule)
		{
			NewActive = t;
			break;
		}
	}
	if (NewActive == ActiveTrack)
		return;

	ActiveTrack = NewActive;
	// 편집 대상이 아닌 트랙의 키는 선택에서 뺀다. (흐리게 그려지는 커브를 툴바 버튼이 건드리지 않게)
	for (int32 i = SelectedKeys.Num() - 1; i >= 0; --i)
	{
		if (!IsTrackActive(SelectedKeys[i].Track))
			SelectedKeys.RemoveAt(static_cast<uint32>(i));
	}
}

// ---------------------------------------------------------------------------------------------
// 그리기
// ---------------------------------------------------------------------------------------------

void FParticleCurveEditorPanel::OnRender()
{
	bool bOpen = IsOpen();
	const bool bVisible = ImGui::Begin(PanelName.c_str(), &bOpen);
	SetOpen(bOpen);
	if (!bVisible)
	{
		ImGui::End();
		return;
	}

	SyncTracks();
	UpdateActiveTrack();
	DrawToolbar();
	ImGui::Separator();

	DrawTrackList();
	ImGui::SameLine(0.0f, 0.0f);
	DrawGraph();
	DrawKeyPopup();

	ImGui::End();
}

bool FParticleCurveEditorPanel::ToolButton(const char* Id, UTexture2D* Icon, const char* Label, const char* Tooltip,
	const bool bActive, const bool bEnabled)
{
	// 언리얼 커브 에디터처럼 아이콘 아래에 이름을 붙인 버튼. 켜진 모드는 주황 배경이다.
	const ImGuiStyle& Style = ImGui::GetStyle();
	const float LabelHeight = ImGui::GetTextLineHeight();
	const ImVec2 TextSize = ImGui::CalcTextSize(Label);
	// 라벨이 긴 버튼(Horizontal, Straighten 등)은 라벨 폭만큼 넓힌다.
	const ImVec2 Size(std::max(ToolButtonWidth, TextSize.x + 10.0f), ToolIconSize + LabelHeight + Style.FramePadding.y * 2.0f + 2.0f);
	const ImVec2 Min = ImGui::GetCursorScreenPos();
	ImGui::BeginDisabled(!bEnabled);
	const bool bClicked = ImGui::InvisibleButton(Id, Size);
	const bool bHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
	ImGui::EndDisabled();

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 Max(Min.x + Size.x, Min.y + Size.y);
	if (bActive)
		DrawList->AddRectFilled(Min, Max, ActiveButton, 3.0f);
	else if (bHovered && bEnabled)
		DrawList->AddRectFilled(Min, Max, ImGui::GetColorU32(ImGuiCol_ButtonHovered), 3.0f);

	const float Alpha = bEnabled ? 1.0f : 0.35f;
	if (Icon && Icon->GetResource())
	{
		const ImVec2 IconMin(Min.x + (Size.x - ToolIconSize) * 0.5f, Min.y + Style.FramePadding.y);
		DrawList->AddImage(reinterpret_cast<ImTextureID>(Icon->GetResource()->GetSRV()), IconMin,
			ImVec2(IconMin.x + ToolIconSize, IconMin.y + ToolIconSize), ImVec2(0, 0), ImVec2(1, 1),
			IM_COL32(255, 255, 255, static_cast<int>(255 * Alpha)));
	}
	DrawList->AddText(ImVec2(Min.x + (Size.x - TextSize.x) * 0.5f, Max.y - LabelHeight - Style.FramePadding.y),
		IM_COL32(230, 230, 230, static_cast<int>(255 * Alpha)), Label);

	if (bHovered && Tooltip)
		ImGui::SetTooltip("%s", Tooltip);
	ImGui::SameLine(0.0f, 2.0f);
	return bClicked && bEnabled;
}

void FParticleCurveEditorPanel::DrawToolbar()
{
	const auto GroupSeparator = []()
	{
		ImGui::SameLine(0.0f, 6.0f);
		const ImVec2 P = ImGui::GetCursorScreenPos();
		ImGui::GetWindowDrawList()->AddLine(ImVec2(P.x, P.y + 4.0f), ImVec2(P.x, P.y + 50.0f), IM_COL32(90, 90, 90, 255));
		ImGui::Dummy(ImVec2(1.0f, 1.0f));
		ImGui::SameLine(0.0f, 6.0f);
	};
	const bool bHasSelection = !SelectedKeys.IsEmpty();

	if (ToolButton("##FitH", Icons.FitHorizontal, "Horizontal", "Fit the horizontal range to the visible curves"))
		FitView(true, false, false);
	if (ToolButton("##FitV", Icons.FitVertical, "Vertical", "Fit the vertical range to the visible curves"))
		FitView(false, true, false);
	if (ToolButton("##Fit", Icons.FitSelected, "Fit", "Fit the view to the selected keys (all curves if none selected)"))
		FitView(true, true, true);
	GroupSeparator();

	if (ToolButton("##Pan", Icons.Pan, "Pan", "Left-drag on empty space pans the view", MouseMode == EMouseMode::Pan))
		MouseMode = EMouseMode::Pan;
	if (ToolButton("##Zoom", Icons.Zoom, "Zoom", "Left-drag on empty space zooms the view", MouseMode == EMouseMode::Zoom))
		MouseMode = EMouseMode::Zoom;
	GroupSeparator();

	const char* NeedKey = "Select keys first";
	if (ToolButton("##Auto", Icons.Auto, "Auto", bHasSelection ? "Auto tangents" : NeedKey, false, bHasSelection))
		SetSelectedInterpMode(EInterpCurveMode::CurveAuto);
	if (ToolButton("##AutoClamped", Icons.AutoClamped, "Auto/Clamp", bHasSelection ? "Auto tangents, flat at peaks (no overshoot)" : NeedKey, false, bHasSelection))
		SetSelectedInterpMode(EInterpCurveMode::CurveAutoClamped);
	if (ToolButton("##User", Icons.User, "User", bHasSelection ? "User tangents (arrive = leave)" : NeedKey, false, bHasSelection))
		SetSelectedInterpMode(EInterpCurveMode::CurveUser);
	if (ToolButton("##Break", Icons.Break, "Break", bHasSelection ? "Broken tangents (arrive and leave edited separately)" : NeedKey, false, bHasSelection))
		SetSelectedInterpMode(EInterpCurveMode::CurveBreak);
	if (ToolButton("##Linear", Icons.Linear, "Linear", bHasSelection ? "Straight line to the next key" : NeedKey, false, bHasSelection))
		SetSelectedInterpMode(EInterpCurveMode::Linear);
	if (ToolButton("##Constant", Icons.Constant, "Constant", bHasSelection ? "Hold the value until the next key" : NeedKey, false, bHasSelection))
		SetSelectedInterpMode(EInterpCurveMode::Constant);
	GroupSeparator();

	if (ToolButton("##Flatten", Icons.Flatten, "Flatten", bHasSelection ? "Set tangents to zero" : NeedKey, false, bHasSelection))
		FlattenSelectedTangents();
	if (ToolButton("##Straighten", Icons.Straighten, "Straighten", bHasSelection ? "Make arrive and leave tangents equal" : NeedKey, false, bHasSelection))
		StraightenSelectedTangents();
	if (ToolButton("##ShowAll", Icons.ShowAll, "Show All", "Show tangent handles on every curve key", bShowAllTangents))
		bShowAllTangents = !bShowAllTangents;
	GroupSeparator();

	if (ToolButton("##CreateTab", Icons.CreateTab, "Create", "Create a new curve tab"))
	{
		FCurveTab Tab;
		Tab.Name = "Tab " + std::to_string(NextTabNumber++);
		Tabs.Add(Tab);
		CurrentTabIndex = Tabs.Num() - 1;
		SelectedKeys.Reset();
		LastSelectedModule = nullptr;
	}
	if (ToolButton("##DeleteTab", Icons.DeleteTab, "Delete", CurrentTabIndex > 0 ? "Delete the current curve tab" : "The Default tab cannot be deleted",
		false, CurrentTabIndex > 0))
	{
		Tabs.RemoveAt(static_cast<uint32>(CurrentTabIndex));
		CurrentTabIndex = std::max(0, CurrentTabIndex - 1);
		SelectedKeys.Reset();
	}

	// 현재 탭 선택. 패널이 좁아 오른쪽에 자리가 없으면 다음 줄로 내린다.
	ImGui::SameLine(0.0f, 10.0f);
	if (ImGui::GetContentRegionAvail().x < 170.0f)
		ImGui::NewLine();
	ImGui::BeginGroup();
	ImGui::TextUnformatted("Current Tab:");
	ImGui::SetNextItemWidth(160.0f);
	if (ImGui::BeginCombo("##CurveTab", CurrentTab().Name.c_str()))
	{
		for (int32 i = 0; i < Tabs.Num(); ++i)
		{
			if (ImGui::Selectable(Tabs[i].Name.c_str(), i == CurrentTabIndex) && i != CurrentTabIndex)
			{
				CurrentTabIndex = i;
				SelectedKeys.Reset();
			}
		}
		ImGui::EndCombo();
	}
	ImGui::EndGroup();
}

void FParticleCurveEditorPanel::DrawTrackList()
{
	ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(140, 140, 140, 255));
	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(20, 20, 20, 255));
	ImGui::BeginChild("##CurveTracks", ImVec2(TrackListWidth, 0.0f), ImGuiChildFlags_None);

	FCurveTab& Tab = CurrentTab();
	if (Tab.Tracks.IsEmpty())
	{
		ImGui::TextWrapped("Select a module with curves (Size, Color) in the Emitters panel to add it here.");
	}

	int32 RemoveIndex = INDEX_NONE;
	for (int32 t = 0; t < Tab.Tracks.Num(); ++t)
	{
		FTrack& Track = Tab.Tracks[t];
		ImGui::PushID(t);
		TArray<FParticleCurveRef> Curves;
		Track.Module->GetCurves(Curves);
		if (Track.SubVisible.Num() != Curves.Num())
		{
			Track.SubVisible.SetNum(Curves.Num());
			for (int32 s = 0; s < Curves.Num(); ++s)
				Track.SubVisible[s] = 1;
		}

		// 트랙 머리: 왼쪽 색 막대(트랙 색) + 모듈 이름. 클릭하면 이 트랙이 편집 대상이 되고 이미터 패널에서도 그 모듈을 고른다.
		// 편집 대상 트랙은 머리 배경을 트랙 색으로 칠한다. (그래프에서 진하게 그려지는 커브가 이 트랙 것이다)
		const bool bModuleSelected = Context.SelectedModule == Track.Module;
		const ImVec2 HeaderMin = ImGui::GetCursorScreenPos();
		const float HeaderHeight = ImGui::GetTextLineHeightWithSpacing();
		ImDrawList* HeaderDraw = ImGui::GetWindowDrawList();
		if (bModuleSelected)
			HeaderDraw->AddRectFilled(HeaderMin, ImVec2(HeaderMin.x + ImGui::GetContentRegionAvail().x, HeaderMin.y + HeaderHeight), GetTrackColor(t, 0.85f));
		HeaderDraw->AddRectFilled(HeaderMin, ImVec2(HeaderMin.x + 5.0f, HeaderMin.y + HeaderHeight), GetTrackColor(t));
		ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
		ImGui::Indent(8.0f);
		if (ImGui::Selectable(Track.Label.c_str(), bModuleSelected))
		{
			Context.SelectedModule = Track.Module;
			LastSelectedModule = Track.Module;
		}
		ImGui::Unindent(8.0f);
		ImGui::PopStyleColor();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(bModuleSelected ? "Editing this module's curves" : "Click to edit this module's curves (others are dimmed)");
		if (ImGui::BeginPopupContextItem("##TrackMenu"))
		{
			if (ImGui::MenuItem("Remove from Curve Editor"))
				RemoveIndex = t;
			ImGui::EndPopup();
		}

		// 표시 상자 줄 (Cascade처럼): 왼쪽은 서브 커브 색 상자를 가로로 나열해 하나씩 켜고 끄고,
		// 오른쪽 끝 상자는 트랙 전체를 켜고 끈다. 꺼진 상자는 회색으로 비워 그린다.
		const float Box = ImGui::GetTextLineHeight() - 4.0f;
		const ImU32 BoxOff = IM_COL32(90, 90, 90, 255);
		const ImU32 BoxBorder = IM_COL32(30, 30, 30, 255);
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		ImGui::Indent(8.0f);
		for (int32 s = 0; s < Curves.Num(); ++s)
		{
			ImGui::PushID(s);
			if (s > 0)
				ImGui::SameLine(0.0f, 3.0f);
			const ImVec2 P = ImGui::GetCursorScreenPos();
			if (ImGui::InvisibleButton("##SubVis", ImVec2(Box, Box)))
				Track.SubVisible[s] = Track.SubVisible[s] ? 0 : 1;
			const bool bOn = Track.SubVisible[s] != 0;
			DrawList->AddRectFilled(P, ImVec2(P.x + Box, P.y + Box), bOn ? ToImColor(Curves[s].Color) : BoxOff);
			DrawList->AddRect(P, ImVec2(P.x + Box, P.y + Box), BoxBorder);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s\n(click to %s)", Curves[s].Name.c_str(), bOn ? "hide" : "show");
			ImGui::PopID();
		}
		ImGui::Unindent(8.0f);

		// 트랙 전체 상자: 오른쪽 끝에 붙인다.
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - Box - 4.0f);
		{
			const ImVec2 P = ImGui::GetCursorScreenPos();
			if (ImGui::InvisibleButton("##TrackVis", ImVec2(Box, Box)))
				Track.bTrackVisible = !Track.bTrackVisible;
			DrawList->AddRectFilled(P, ImVec2(P.x + Box, P.y + Box), Track.bTrackVisible ? GetTrackColor(t) : BoxOff);
			DrawList->AddRect(P, ImVec2(P.x + Box, P.y + Box), BoxBorder);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(Track.bTrackVisible ? "Hide all curves of this track" : "Show this track's curves");
		}

		// 트랙 사이 구분선
		const ImVec2 SepPos = ImGui::GetCursorScreenPos();
		DrawList->AddLine(ImVec2(SepPos.x - ImGui::GetStyle().WindowPadding.x, SepPos.y + 1.0f),
			ImVec2(SepPos.x + ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x, SepPos.y + 1.0f), IM_COL32(60, 60, 60, 255));
		ImGui::Dummy(ImVec2(0.0f, 3.0f));
		ImGui::PopID();
	}
	if (RemoveIndex != INDEX_NONE)
	{
		Tab.Tracks.RemoveAt(static_cast<uint32>(RemoveIndex));
		SelectedKeys.Reset();
	}

	ImGui::EndChild();
	ImGui::PopStyleColor(2);
}

void FParticleCurveEditorPanel::DrawGrid(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max)
{
	DrawList->AddRectFilled(Min, Max, GraphBackground);

	const float StepX = NiceGridStep(ViewMaxX - ViewMinX, Max.x - Min.x, 70.0f);
	const float StepY = NiceGridStep(ViewMaxY - ViewMinY, Max.y - Min.y, 80.0f);
	char Text[32];

	for (float X = std::ceil(ViewMinX / StepX) * StepX; X <= ViewMaxX; X += StepX)
	{
		const float SX = ToScreen(X, 0.0f).x;
		const bool bAxis = std::abs(X) < StepX * 0.5f;
		DrawList->AddLine(ImVec2(SX, Min.y), ImVec2(SX, Max.y), bAxis ? GridAxis : GridLine);
		snprintf(Text, sizeof(Text), "%.2f", std::abs(X) < 1e-6f ? 0.0f : X);
		DrawList->AddText(ImVec2(SX + 2.0f, Max.y - ImGui::GetTextLineHeight() - 2.0f), GridText, Text);
	}
	for (float Y = std::ceil(ViewMinY / StepY) * StepY; Y <= ViewMaxY; Y += StepY)
	{
		const float SY = ToScreen(0.0f, Y).y;
		const bool bAxis = std::abs(Y) < StepY * 0.5f;
		DrawList->AddLine(ImVec2(Min.x, SY), ImVec2(Max.x, SY), bAxis ? GridAxis : GridLine);
		snprintf(Text, sizeof(Text), "%.2f", std::abs(Y) < 1e-6f ? 0.0f : Y);
		DrawList->AddText(ImVec2(Min.x + 2.0f, SY - ImGui::GetTextLineHeight() - 1.0f), GridText, Text);
	}
}

void FParticleCurveEditorPanel::DrawGraph()
{
	ImGui::BeginChild("##CurveGraph", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	GraphMin = ImGui::GetCursorScreenPos();
	const ImVec2 Avail = ImGui::GetContentRegionAvail();
	GraphMax = ImVec2(GraphMin.x + std::max(Avail.x, 10.0f), GraphMin.y + std::max(Avail.y, 10.0f));

	ImGui::InvisibleButton("##GraphCanvas", ImVec2(GraphMax.x - GraphMin.x, GraphMax.y - GraphMin.y),
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
	const bool bHovered = ImGui::IsItemHovered();
	const bool bActive = ImGui::IsItemActive();
	ImGuiIO& IO = ImGui::GetIO();
	const ImVec2 Mouse = IO.MousePos;

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->PushClipRect(GraphMin, GraphMax, true);
	DrawGrid(DrawList, GraphMin, GraphMax);

	FCurveTab& Tab = CurrentTab();

	// 이번 프레임의 커브 목록. (모듈 포인터 → 커브 참조)
	const bool bEmitterTime = Cast<UParticleModuleBeamSource>(Context.SelectedModule) ||
	    Cast<UParticleModuleBeamTarget>(Context.SelectedModule);
	DrawList->AddText(ImVec2(GraphMin.x + 50.0f, GraphMax.y - ImGui::GetTextLineHeight() - 4.0f),
	    IM_COL32(220, 220, 220, 230), bEmitterTime ? "X: Emitter time (seconds)" : "X: Relative time");
	struct FVisibleCurve
	{
		int32 Track;
		int32 Sub;
		FParticleCurveRef Ref;
		// 편집 대상 트랙의 커브인지. 아니면 흐리게 그리고 키를 잡을 수 없다.
		bool bActive;
	};
	TArray<FVisibleCurve> Visible;
	for (int32 t = 0; t < Tab.Tracks.Num(); ++t)
	{
		TArray<FParticleCurveRef> Curves;
		Tab.Tracks[t].Module->GetCurves(Curves);
		for (int32 s = 0; s < Curves.Num(); ++s)
		{
			if (Tab.Tracks[t].IsSubVisible(s) && Curves[s].Curve)
				Visible.Add({ t, s, Curves[s], IsTrackActive(t) });
		}
	}

	// 마우스 아래(세로 거리 CurvePickDistance 안)에서 가장 가까운 커브. bActiveOnly면 편집 대상 트랙만 본다.
	const auto FindCurveNearMouse = [&](bool bActiveOnly) -> const FVisibleCurve*
	{
		const float X = ToCurveX(Mouse.x);
		float BestDist = CurvePickDistance;
		const FVisibleCurve* Best = nullptr;
		for (const FVisibleCurve& VC : Visible)
		{
			if (bActiveOnly && !VC.bActive)
				continue;
			const float Dist = std::abs(ToScreen(X, VC.Ref.Curve->Eval(X, 1.0f)).y - Mouse.y);
			if (Dist < BestDist)
			{
				BestDist = Dist;
				Best = &VC;
			}
		}
		return Best;
	};

	// ---- 1. 입력: 손잡이 → 키 → 빈 곳 순서로 맞힌다 ----
	const bool bCtrl = IO.KeyCtrl;
	if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		Drag = EDrag::None;
		// 1-1. 탄젠트 손잡이
		for (const FVisibleCurve& VC : Visible)
		{
			if (!VC.bActive)
				continue;
			const TArray<FInterpCurvePointFloat>& Points = VC.Ref.Curve->Points;
			for (int32 k = 0; k < Points.Num() && Drag == EDrag::None; ++k)
			{
				const FKeyRef Ref{ VC.Track, VC.Sub, k };
				if (!ShouldShowTangents(Ref, Points[k]))
					continue;
				const ImVec2 KeyPos = ToScreen(Points[k].InVal, Points[k].OutVal);
				for (const bool bLeave : { false, true })
				{
					const ImVec2 Off = TangentHandleOffset(bLeave ? Points[k].LeaveTangent : Points[k].ArriveTangent, bLeave);
					const float DX = Mouse.x - (KeyPos.x + Off.x), DY = Mouse.y - (KeyPos.y + Off.y);
					if (DX * DX + DY * DY <= KeyHitRadius * KeyHitRadius)
					{
						Drag = bLeave ? EDrag::LeaveTangent : EDrag::ArriveTangent;
						DragTangentKey = Ref;
						break;
					}
				}
			}
			if (Drag != EDrag::None)
				break;
		}

		// 1-2. 키
		if (Drag == EDrag::None)
		{
			FKeyRef Hit;
			for (const FVisibleCurve& VC : Visible)
			{
				if (!VC.bActive)
					continue;
				const TArray<FInterpCurvePointFloat>& Points = VC.Ref.Curve->Points;
				for (int32 k = 0; k < Points.Num(); ++k)
				{
					const ImVec2 KeyPos = ToScreen(Points[k].InVal, Points[k].OutVal);
					if (std::abs(Mouse.x - KeyPos.x) <= KeyHitRadius && std::abs(Mouse.y - KeyPos.y) <= KeyHitRadius)
						Hit = { VC.Track, VC.Sub, k };
				}
			}
			if (Hit.Key >= 0)
			{
				const int32 Existing = SelectedKeys.Find(Hit);
				if (bCtrl)
				{
					if (Existing != INDEX_NONE) SelectedKeys.RemoveAt(static_cast<uint32>(Existing));
					else SelectedKeys.Add(Hit);
				}
				else if (Existing == INDEX_NONE)
				{
					SelectedKeys.Reset();
					SelectedKeys.Add(Hit);
				}
				Drag = SelectedKeys.Find(Hit) != INDEX_NONE ? EDrag::Keys : EDrag::None;
			}
			// 1-3. Ctrl+클릭: 가까운 곡선 위에 키 추가
			else if (bCtrl)
			{
				if (const FVisibleCurve* Best = FindCurveNearMouse(true))
				{
					const float X = ToCurveX(Mouse.x);
					const int32 NewKey = Best->Ref.Curve->AddPoint(X, Best->Ref.Curve->Eval(X, 1.0f));
					AddLinkedPoints(Best->Track, Best->Sub, X); // R에 키를 넣으면 G/B에도 같은 시간 키
					SelectedKeys.Reset();
					SelectedKeys.Add({ Best->Track, Best->Sub, NewKey });
					MarkEdited();
				}
			}
			// 1-4. 흐리게 그려진(편집 대상이 아닌) 커브를 누르면 그 트랙을 편집 대상으로 바꾼다.
			else if (const FVisibleCurve* Other = FindCurveNearMouse(false); Other && !Other->bActive)
			{
				Context.SelectedModule = Tab.Tracks[Other->Track].Module;
				LastSelectedModule = Context.SelectedModule;
				SelectedKeys.Reset();
			}
			// 1-5. 빈 곳: 선택 해제 후 패닝/줌
			else
			{
				SelectedKeys.Reset();
				Drag = MouseMode == EMouseMode::Pan ? EDrag::Pan : EDrag::Zoom;
			}
		}
	}

	if (bActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
	{
		const float UnitsPerPixelX = (ViewMaxX - ViewMinX) / std::max(1.0f, GraphMax.x - GraphMin.x);
		const float UnitsPerPixelY = (ViewMaxY - ViewMinY) / std::max(1.0f, GraphMax.y - GraphMin.y);
		if (Drag == EDrag::Keys)
		{
			// 가로(시간)는 연결된 커브의 같은 시간 키까지 같은 양만큼 옮긴다. 세로(값)는 고른 키만 바꾼다.
			TArray<FKeyRef> MoveKeys = SelectedKeys;
			for (const FKeyRef& Ref : SelectedKeys)
				AppendLinkedKeys(Ref, MoveKeys);

			// 함께 옮기는 키는 이웃 키를 넘지 않게 이동량을 줄인다. (같이 움직이는 이웃은 막지 않는다. 선택 인덱스가 바뀌지 않는다)
			float DX = IO.MouseDelta.x * UnitsPerPixelX;
			for (const FKeyRef& Ref : MoveKeys)
			{
				FInterpCurveFloat* Curve = GetCurve(Ref);
				if (!Curve || !Curve->Points.IsValidIndex(Ref.Key))
					continue;
				const TArray<FInterpCurvePointFloat>& Points = Curve->Points;
				const float InVal = Points[Ref.Key].InVal;
				if (Ref.Key > 0 && MoveKeys.Find({ Ref.Track, Ref.Sub, Ref.Key - 1 }) == INDEX_NONE)
					DX = std::max(DX, std::min(0.0f, Points[Ref.Key - 1].InVal + 1e-3f - InVal));
				if (Ref.Key + 1 < Points.Num() && MoveKeys.Find({ Ref.Track, Ref.Sub, Ref.Key + 1 }) == INDEX_NONE)
					DX = std::min(DX, std::max(0.0f, Points[Ref.Key + 1].InVal - 1e-3f - InVal));
			}
			for (const FKeyRef& Ref : MoveKeys)
			{
				FInterpCurveFloat* Curve = GetCurve(Ref);
				if (!Curve || !Curve->Points.IsValidIndex(Ref.Key))
					continue;
				FInterpCurvePointFloat& P = Curve->Points[Ref.Key];
				P.InVal += DX;
				if (SelectedKeys.Find(Ref) != INDEX_NONE)
					P.OutVal -= IO.MouseDelta.y * UnitsPerPixelY;
				Curve->AutoSetTangents();
			}
			if (IO.MouseDelta.x != 0.0f || IO.MouseDelta.y != 0.0f)
				MarkEdited();
		}
		else if (Drag == EDrag::ArriveTangent || Drag == EDrag::LeaveTangent)
		{
			FInterpCurveFloat* Curve = GetCurve(DragTangentKey);
			if (Curve && Curve->Points.IsValidIndex(DragTangentKey.Key))
			{
				FInterpCurvePointFloat& P = Curve->Points[DragTangentKey.Key];
				const ImVec2 KeyPos = ToScreen(P.InVal, P.OutVal);
				float DX = (Mouse.x - KeyPos.x) * UnitsPerPixelX;
				const float DY = -(Mouse.y - KeyPos.y) * UnitsPerPixelY;
				// 나가는 손잡이는 오른쪽, 들어오는 손잡이는 왼쪽에만 둔다. (세로로 세우면 기울기가 무한대가 된다)
				const float MinDX = 1e-4f * (ViewMaxX - ViewMinX);
				DX = Drag == EDrag::LeaveTangent ? std::max(DX, MinDX) : std::min(DX, -MinDX);
				const float Tangent = DY / DX;
				// 자동 탄젠트 키를 손으로 바꾸면 사용자 탄젠트로 바뀐다.
				if (P.InterpMode == EInterpCurveMode::CurveAuto || P.InterpMode == EInterpCurveMode::CurveAutoClamped)
					P.InterpMode = EInterpCurveMode::CurveUser;
				if (P.InterpMode == EInterpCurveMode::CurveBreak)
				{
					(Drag == EDrag::LeaveTangent ? P.LeaveTangent : P.ArriveTangent) = Tangent;
				}
				else
				{
					P.ArriveTangent = P.LeaveTangent = Tangent;
				}
				Curve->AutoSetTangents();
				MarkEdited();
			}
		}
		else if (Drag == EDrag::Pan)
		{
			const float DX = IO.MouseDelta.x * UnitsPerPixelX, DY = IO.MouseDelta.y * UnitsPerPixelY;
			ViewMinX -= DX; ViewMaxX -= DX; ViewMinY += DY; ViewMaxY += DY;
		}
		else if (Drag == EDrag::Zoom)
		{
			// 오른쪽·위로 끌면 확대, 왼쪽·아래로 끌면 축소. 가운데를 기준으로 한다.
			const float ZX = std::exp(-IO.MouseDelta.x * 0.005f), ZY = std::exp(IO.MouseDelta.y * 0.005f);
			const float CX = (ViewMinX + ViewMaxX) * 0.5f, CY = (ViewMinY + ViewMaxY) * 0.5f;
			ViewMinX = CX + (ViewMinX - CX) * ZX; ViewMaxX = CX + (ViewMaxX - CX) * ZX;
			ViewMinY = CY + (ViewMinY - CY) * ZY; ViewMaxY = CY + (ViewMaxY - CY) * ZY;
		}
	}
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		Drag = EDrag::None;

	// 우·휠 드래그는 항상 패닝
	if (bActive && (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 2.0f) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)))
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
			bRightDragged = true;
		const float DX = IO.MouseDelta.x * (ViewMaxX - ViewMinX) / std::max(1.0f, GraphMax.x - GraphMin.x);
		const float DY = IO.MouseDelta.y * (ViewMaxY - ViewMinY) / std::max(1.0f, GraphMax.y - GraphMin.y);
		ViewMinX -= DX; ViewMaxX -= DX; ViewMinY += DY; ViewMaxY += DY;
	}
	// 키 우클릭(드래그 없이 놓음): 값 입력 팝업
	if (bHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
	{
		if (!bRightDragged)
		{
			for (const FVisibleCurve& VC : Visible)
			{
				if (!VC.bActive)
					continue;
				const TArray<FInterpCurvePointFloat>& Points = VC.Ref.Curve->Points;
				for (int32 k = 0; k < Points.Num(); ++k)
				{
					const ImVec2 KeyPos = ToScreen(Points[k].InVal, Points[k].OutVal);
					if (std::abs(Mouse.x - KeyPos.x) <= KeyHitRadius && std::abs(Mouse.y - KeyPos.y) <= KeyHitRadius)
					{
						PopupKey = { VC.Track, VC.Sub, k };
						bOpenKeyPopup = true;
					}
				}
			}
		}
		bRightDragged = false;
	}

	// 휠: 마우스 위치를 기준으로 확대·축소
	if (bHovered && IO.MouseWheel != 0.0f)
	{
		const float Zoom = std::pow(0.85f, IO.MouseWheel);
		const float MX = ToCurveX(Mouse.x), MY = ToCurveY(Mouse.y);
		ViewMinX = MX + (ViewMinX - MX) * Zoom; ViewMaxX = MX + (ViewMaxX - MX) * Zoom;
		ViewMinY = MY + (ViewMinY - MY) * Zoom; ViewMaxY = MY + (ViewMaxY - MY) * Zoom;
	}

	// Delete: 선택 키 삭제
	if ((bHovered || ImGui::IsWindowFocused()) && !SelectedKeys.IsEmpty() && !IO.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
		DeleteSelectedKeys();

	// 너무 작거나 큰 범위로 가지 않게 묶는다.
	if (ViewMaxX - ViewMinX < 1e-3f) ViewMaxX = ViewMinX + 1e-3f;
	if (ViewMaxY - ViewMinY < 1e-3f) ViewMaxY = ViewMinY + 1e-3f;

	// ---- 2. 곡선 ----
	// 편집 대상이 아닌 트랙은 가늘고 흐리게 먼저 그리고, 편집 대상 트랙을 그 위에 굵게 그린다.
	for (const bool bDrawActive : { false, true })
	{
		for (const FVisibleCurve& VC : Visible)
		{
			if (VC.bActive != bDrawActive)
				continue;
			const ImU32 Color = ToImColor(VC.Ref.Color, VC.bActive ? 1.0f : 0.22f);
			const float Thickness = VC.bActive ? 2.5f : 1.0f;
			const float Step = 3.0f;
			ImVec2 Prev = ToScreen(ToCurveX(GraphMin.x), VC.Ref.Curve->Eval(ToCurveX(GraphMin.x), 1.0f));
			for (float SX = GraphMin.x + Step; SX <= GraphMax.x + Step; SX += Step)
			{
				const float X = ToCurveX(SX);
				const ImVec2 Cur = ToScreen(X, VC.Ref.Curve->Eval(X, 1.0f));
				DrawList->AddLine(Prev, Cur, Color, Thickness);
				Prev = Cur;
			}
		}
	}

	// ---- 2-1. 곡선 오른쪽 끝 이름표: "트랙 색 칩 + 커브 이름". 같은 높이면 아래로 밀어 겹치지 않게 한다 ----
	{
		const float LineHeight = ImGui::GetTextLineHeight();
		TArray<float> UsedY;
		for (const bool bDrawActive : { true, false })
		{
			for (const FVisibleCurve& VC : Visible)
			{
				if (VC.bActive != bDrawActive)
					continue;
				const FString Label = Tab.Tracks[VC.Track].Label.substr(0, Tab.Tracks[VC.Track].Label.find(':')) + " " + VC.Ref.Name;
				const ImVec2 TextSize = ImGui::CalcTextSize(Label.c_str());
				const float RightX = GraphMax.x - 6.0f;
				float Y = ToScreen(ViewMaxX, VC.Ref.Curve->Eval(ViewMaxX, 1.0f)).y - LineHeight - 2.0f;
				Y = std::clamp(Y, GraphMin.y + 2.0f, GraphMax.y - LineHeight * 2.0f);
				for (bool bMoved = true; bMoved;)
				{
					bMoved = false;
					for (const float Used : UsedY)
					{
						if (std::abs(Used - Y) < LineHeight + 1.0f)
						{
							Y = Used + LineHeight + 1.0f;
							bMoved = true;
						}
					}
				}
				UsedY.Add(Y);
				const float Alpha = VC.bActive ? 1.0f : 0.35f;
				const ImVec2 ChipMin(RightX - TextSize.x - 12.0f, Y);
				DrawList->AddRectFilled(ImVec2(ChipMin.x - 2.0f, Y - 1.0f), ImVec2(RightX + 2.0f, Y + LineHeight + 1.0f), IM_COL32(30, 30, 30, static_cast<int>(200 * Alpha)), 3.0f);
				DrawList->AddRectFilled(ChipMin, ImVec2(ChipMin.x + 8.0f, Y + LineHeight), GetTrackColor(VC.Track, Alpha), 2.0f);
				DrawList->AddText(ImVec2(ChipMin.x + 12.0f, Y), ToImColor(VC.Ref.Color, Alpha), Label.c_str());
			}
		}
	}

	// ---- 3. 탄젠트 손잡이와 키 (편집 대상 트랙만) ----
	for (const FVisibleCurve& VC : Visible)
	{
		if (!VC.bActive)
			continue;
		const TArray<FInterpCurvePointFloat>& Points = VC.Ref.Curve->Points;
		const ImU32 Color = ToImColor(VC.Ref.Color);
		for (int32 k = 0; k < Points.Num(); ++k)
		{
			const FKeyRef Ref{ VC.Track, VC.Sub, k };
			const ImVec2 KeyPos = ToScreen(Points[k].InVal, Points[k].OutVal);
			if (ShouldShowTangents(Ref, Points[k]))
			{
				for (const bool bLeave : { false, true })
				{
					const ImVec2 Off = TangentHandleOffset(bLeave ? Points[k].LeaveTangent : Points[k].ArriveTangent, bLeave);
					const ImVec2 Handle(KeyPos.x + Off.x, KeyPos.y + Off.y);
					DrawList->AddLine(KeyPos, Handle, TangentColor, 1.0f);
					DrawList->AddCircleFilled(Handle, 3.5f, TangentColor);
				}
			}
			const bool bSelected = SelectedKeys.Find(Ref) != INDEX_NONE;
			const float Half = bSelected ? KeyHalfSize + 1.0f : KeyHalfSize;
			DrawList->AddRectFilled(ImVec2(KeyPos.x - Half, KeyPos.y - Half), ImVec2(KeyPos.x + Half, KeyPos.y + Half),
				bSelected ? SelectedKeyColor : Color);
			DrawList->AddRect(ImVec2(KeyPos.x - Half, KeyPos.y - Half), ImVec2(KeyPos.x + Half, KeyPos.y + Half), IM_COL32(20, 20, 20, 255));
		}
	}

	// 마우스 오버 툴팁: 키(편집 대상) 또는 곡선이 어느 트랙·커브인지 보여 준다.
	if (bHovered && Drag == EDrag::None && !ImGui::IsAnyMouseDown())
	{
		const FVisibleCurve* HoverCurve = nullptr;
		int32 HoverKey = -1;
		for (const FVisibleCurve& VC : Visible)
		{
			if (!VC.bActive)
				continue;
			const TArray<FInterpCurvePointFloat>& Points = VC.Ref.Curve->Points;
			for (int32 k = 0; k < Points.Num(); ++k)
			{
				const ImVec2 KeyPos = ToScreen(Points[k].InVal, Points[k].OutVal);
				if (std::abs(Mouse.x - KeyPos.x) <= KeyHitRadius && std::abs(Mouse.y - KeyPos.y) <= KeyHitRadius)
				{
					HoverCurve = &VC;
					HoverKey = k;
				}
			}
		}
		if (!HoverCurve)
			HoverCurve = FindCurveNearMouse(false);
		if (HoverCurve)
		{
			const FString& TrackLabel = Tab.Tracks[HoverCurve->Track].Label;
			if (HoverKey >= 0)
			{
				const FInterpCurvePointFloat& P = HoverCurve->Ref.Curve->Points[HoverKey];
				ImGui::SetTooltip("%s - %s\nTime %.3f   Value %.3f   %s", TrackLabel.c_str(), HoverCurve->Ref.Name.c_str(),
					P.InVal, P.OutVal, GetInterpModeName(P.InterpMode));
			}
			else
			{
				ImGui::SetTooltip(HoverCurve->bActive ? "%s - %s" : "%s - %s\n(click to edit this track)",
					TrackLabel.c_str(), HoverCurve->Ref.Name.c_str());
			}
		}
	}

	// 선택 키 정보 (왼쪽 위)
	if (SelectedKeys.Num() == 1)
	{
		const FInterpCurveFloat* Curve = GetCurve(SelectedKeys[0]);
		if (Curve && Curve->Points.IsValidIndex(SelectedKeys[0].Key))
		{
			const FInterpCurvePointFloat& P = Curve->Points[SelectedKeys[0].Key];
			char Info[96];
			snprintf(Info, sizeof(Info), "Time %.3f   Value %.3f   %s", P.InVal, P.OutVal, GetInterpModeName(P.InterpMode));
			DrawList->AddText(ImVec2(GraphMin.x + 50.0f, GraphMin.y + 4.0f), IM_COL32(255, 255, 255, 230), Info);
		}
	}
	else if (Visible.IsEmpty())
	{
		DrawList->AddText(ImVec2(GraphMin.x + 50.0f, GraphMin.y + 4.0f), IM_COL32(220, 220, 220, 200),
			"Ctrl+Click on a curve to add a key. Drag keys to move them. Right-click a key to type values.");
	}

	DrawList->PopClipRect();
	ImGui::EndChild();
}

void FParticleCurveEditorPanel::DrawKeyPopup()
{
	if (bOpenKeyPopup)
	{
		ImGui::OpenPopup("##CurveKeyPopup");
		bOpenKeyPopup = false;
	}
	if (!ImGui::BeginPopup("##CurveKeyPopup"))
		return;

	FInterpCurveFloat* Curve = GetCurve(PopupKey);
	if (!Curve || !Curve->Points.IsValidIndex(PopupKey.Key))
	{
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}

	FInterpCurvePointFloat& P = Curve->Points[PopupKey.Key];
	bool bChanged = false;
	ImGui::SetNextItemWidth(140.0f);
	float Time = P.InVal;
	if (ImGui::InputFloat("Time", &Time, 0.0f, 0.0f, "%.3f"))
	{
		// 연결된 커브의 같은 시간 키도 같은 시간으로 옮긴다. 모든 키가 이웃을 넘지 않는 범위로 묶는다.
		TArray<FKeyRef> TimeKeys;
		TimeKeys.Add(PopupKey);
		AppendLinkedKeys(PopupKey, TimeKeys);
		float Lo = -FLT_MAX, Hi = FLT_MAX;
		for (const FKeyRef& Ref : TimeKeys)
		{
			const FInterpCurveFloat* C = GetCurve(Ref);
			if (!C || !C->Points.IsValidIndex(Ref.Key))
				continue;
			if (Ref.Key > 0)
				Lo = std::max(Lo, C->Points[Ref.Key - 1].InVal + 1e-3f);
			if (Ref.Key + 1 < C->Points.Num())
				Hi = std::min(Hi, C->Points[Ref.Key + 1].InVal - 1e-3f);
		}
		const float NewTime = std::max(Lo, std::min(Time, Hi));
		for (const FKeyRef& Ref : TimeKeys)
		{
			FInterpCurveFloat* C = GetCurve(Ref);
			if (!C || !C->Points.IsValidIndex(Ref.Key))
				continue;
			C->Points[Ref.Key].InVal = NewTime;
			if (C != Curve)
				C->AutoSetTangents();
		}
		bChanged = true;
	}
	ImGui::SetNextItemWidth(140.0f);
	bChanged |= ImGui::InputFloat("Value", &P.OutVal, 0.0f, 0.0f, "%.3f");

	ImGui::SetNextItemWidth(140.0f);
	if (ImGui::BeginCombo("Interp", GetInterpModeName(P.InterpMode)))
	{
		for (const EInterpCurveMode Mode : { EInterpCurveMode::CurveAuto, EInterpCurveMode::CurveAutoClamped, EInterpCurveMode::CurveUser,
			EInterpCurveMode::CurveBreak, EInterpCurveMode::Linear, EInterpCurveMode::Constant })
		{
			if (ImGui::Selectable(GetInterpModeName(Mode), P.InterpMode == Mode))
			{
				P.InterpMode = Mode;
				bChanged = true;
			}
		}
		ImGui::EndCombo();
	}
	if (P.InterpMode == EInterpCurveMode::CurveUser)
	{
		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::InputFloat("Tangent", &P.LeaveTangent, 0.0f, 0.0f, "%.3f"))
		{
			P.ArriveTangent = P.LeaveTangent;
			bChanged = true;
		}
	}
	else if (P.InterpMode == EInterpCurveMode::CurveBreak)
	{
		ImGui::SetNextItemWidth(140.0f);
		bChanged |= ImGui::InputFloat("Arrive", &P.ArriveTangent, 0.0f, 0.0f, "%.3f");
		ImGui::SetNextItemWidth(140.0f);
		bChanged |= ImGui::InputFloat("Leave", &P.LeaveTangent, 0.0f, 0.0f, "%.3f");
	}

	ImGui::Separator();
	if (ImGui::MenuItem("Delete Key", nullptr, false, Curve->Points.Num() > 1))
	{
		TArray<FKeyRef> Targets;
		Targets.Add(PopupKey);
		AppendLinkedKeys(PopupKey, Targets);
		RemoveKeys(Targets);
		SelectedKeys.Reset();
		MarkEdited();
		ImGui::CloseCurrentPopup();
	}
	else if (bChanged)
	{
		Curve->AutoSetTangents();
		MarkEdited();
	}
	ImGui::EndPopup();
}

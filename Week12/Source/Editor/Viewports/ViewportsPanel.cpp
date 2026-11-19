#include "EnginePCH.h"
#include "Editor/Viewports/ViewportsPanel.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Input/InputSystem.h"
#include "Core/StatOverlay.h"
#include "Core/Stats/LightweightStats.h"
#include "Render/RenderCommand.h"
#include "Editor/EditorUI/ShowFlagUI.h"
#include <algorithm>
#include <cassert>
#include <format>

#include "Core/Stats/EditorStats.h"
#include "Core/Stats/ShadowStats.h"
#include "Core/Stats/DecalStats.h"
#include "Core/Stats/ParticleStats.h"
#include "Core/Stats/SkinningStats.h"
#include "Render/SkinningSettings.h"
#include "Render/ShadowSettings.h"

namespace
{
constexpr ImU32 SplitterColor = IM_COL32(55, 55, 55, 230);
constexpr ImU32 SplitterHoverColor = IM_COL32(255, 192, 0, 255);
constexpr const char* CameraPresetLabels[] = {
	"Perspective", "Ortho (Current)", "Top", "Bottom", "Front", "Back", "Left", "Right"};

// Stat Overlay
constexpr float StatOverlayMargin = 8.0f;
constexpr float StatOverlayPadding = 6.0f;
constexpr ImU32 StatOverlayBackgroundColor = IM_COL32(0, 0, 0, 140);
constexpr ImU32 TitleColor = IM_COL32(255, 210, 60, 255);
constexpr ImU32 ValueColor = IM_COL32(235, 235, 235, 255);
}

FViewportsPanel::FViewportsPanel()
{
	// 그림자 View ID 0은 World 공유 작업용이므로 1부터 준다. 레벨 View는 렌더 타깃을 직접 가진다.
	for (int32 ViewIndex = 0; ViewIndex < ViewCount; ++ViewIndex)
	{
		Viewports[ViewIndex].SetShadowViewId(static_cast<uint32>(ViewIndex + 1));
		Viewports[ViewIndex].SetOwnsRenderTargets(true);
	}
}

bool FViewportsPanel::Init()
{
	return true;
}

void FViewportsPanel::Tick(float DeltaTime)
{
	(void)DeltaTime;
}

FViewport& FViewportsPanel::GetViewport(const int32 ViewIndex)
{
	assert(ViewIndex >= 0 && ViewIndex < ViewCount);
	return Viewports[ViewIndex];
}

const FViewport& FViewportsPanel::GetViewport(const int32 ViewIndex) const
{
	assert(ViewIndex >= 0 && ViewIndex < ViewCount);
	return Viewports[ViewIndex];
}

FViewport* FViewportsPanel::GetActiveViewport()
{
	return IsViewActive(ActiveViewIndex) ? &Viewports[ActiveViewIndex] : nullptr;
}

FViewport* FViewportsPanel::GetEditorViewport()
{
	const int32 Index = GetEditorViewIndex();
	return Index >= 0 && Index < ViewCount ? &Viewports[Index] : nullptr;
}

FViewport* FViewportsPanel::FindViewportByShadowId(const uint32 ShadowViewId)
{
	for (FViewport& Viewport : Viewports)
	{
		if (Viewport.GetShadowViewId() == ShadowViewId)
			return &Viewport;
	}
	return nullptr;
}

// 메인 카메라 투영값을 공유하고 네 View의 기본 프리셋 상태를 만든다.
void FViewportsPanel::InitializeFromWorld(UWorld& World)
{
	UCameraComponent* MainCamera{ World.GetMainCameraComponent() };
	assert(MainCamera != nullptr);

	const FCameraProjection Perspective{
		EProjectionMode::Perspective,
		MainCamera->GetFieldOfView(),
		MainCamera->GetOrthoWidth(),
		MainCamera->GetNearZ(),
		MainCamera->GetFarZ() };
	Layout.SetLayoutMode(ELayoutMode::QuadSplit);
	// 화면 정면은 +X, 화면 오른쪽은 +Y가 되도록 시작한다.
	const FViewCamera InitialCamera{ {{-11.665390f, 6.117728f, 9.921079f},
		{0.081251f, 0.256328f, -0.291035f, 0.918146f}}, Perspective };
	for (int32 ViewIndex = 0; ViewIndex < ViewCount; ++ViewIndex)
		Viewports[ViewIndex].GetClient().SetCamera(InitialCamera, Layout.GetViewRect(ViewIndex));
	Viewports[1].GetClient().ApplyCameraPreset(EMultipleViewportsCameraPreset::Top, Layout.GetViewRect(1));
	Viewports[2].GetClient().ApplyCameraPreset(EMultipleViewportsCameraPreset::Front, Layout.GetViewRect(2));
	Viewports[3].GetClient().ApplyCameraPreset(EMultipleViewportsCameraPreset::Left, Layout.GetViewRect(3));
}

void FViewportsPanel::ApplyCameraProperties(const int32 ViewIndex, const FViewCamera& Camera, const bool bRotationChanged)
{
	GetViewport(ViewIndex).GetClient().ApplyCameraProperties(Camera, bRotationChanged, Layout.GetViewRect(ViewIndex));
}

void FViewportsPanel::ApplyCameraPreset(const int32 ViewIndex, const EMultipleViewportsCameraPreset Preset)
{
	GetViewport(ViewIndex).GetClient().ApplyCameraPreset(Preset, Layout.GetViewRect(ViewIndex));
}

// 배치는 Layout에 맡기고, 각 View에 영역을 넘긴 뒤 입력 대상 유지 규칙을 적용한다.
void FViewportsPanel::UpdateLayout(const FVector2 WindowSize)
{
	Layout.Update(WindowSize, SplitterThickness);
	for (int32 ViewIndex = 0; ViewIndex < ViewCount; ++ViewIndex)
		Viewports[ViewIndex].SetRect(Layout.GetViewRect(ViewIndex), Layout.IsViewActive(ViewIndex));

	// Hover 추적은 비활성화한다. 클릭 위치 판정 함수는 입력 전환에서 계속 사용한다.
	InputState.HoveredViewIndex = InvalidViewIndex;
	if (InputState.CapturedViewIndex != InvalidViewIndex && !IsViewActive(InputState.CapturedViewIndex))
		InputState = EndCapture(InputState);
	// Capture가 없으면 마지막 선택을 유지하고, 숨겨진 View만 레이아웃에 맞게 보정한다.
	if (InputState.CapturedViewIndex != InvalidViewIndex)
		ActiveViewIndex = InputState.CapturedViewIndex;
	if (!IsViewActive(ActiveViewIndex))
	{
		const int32 SingleViewIndex = Layout.GetSingleViewIndex();
		ActiveViewIndex = Layout.GetLayoutMode() == ELayoutMode::Single && IsViewActive(SingleViewIndex)
			? SingleViewIndex : InvalidViewIndex;
	}
}

// Capture와 입력 대상을 결정하고 키·마우스·휠 입력을 해당 View의 Client에 전달한다.
void FViewportsPanel::UpdateInput(const float DeltaTime, const FVector2 LocalMousePosition,
	const float MoveSpeed, const float MouseSensitivity)
{
	const int WheelDelta = FInputSystem::GetWheelDelta(); // 이 Hook은 프레임마다 정확히 한 번만 호출한다.
	if (ImGui::IsKeyReleased(ImGuiKey_MouseRight))
		InputState = EndCapture(InputState);
	const int32 PointerViewIndex = Layout.FindViewAt(LocalMousePosition);
	// 우클릭 시작 위치를 Capture하고, Capture 밖에서는 좌클릭으로만 선택을 바꾼다.
	if (InputState.CapturedViewIndex == InvalidViewIndex && IsViewActive(PointerViewIndex))
	{
		if (FInputSystem::IsMousePressed(EMouseButton::Right))
			InputState = BeginCapture(InputState, PointerViewIndex);
		else if (FInputSystem::IsMousePressed(EMouseButton::Left))
			ActiveViewIndex = PointerViewIndex;
	}
	if (InputState.CapturedViewIndex != InvalidViewIndex)
		ActiveViewIndex = InputState.CapturedViewIndex;
	if (!IsViewActive(ActiveViewIndex))
		return;

	// 키와 마우스 드래그는 우클릭으로 캡처한 View에만 전달한다.
	const bool bCameraCaptured = FInputSystem::IsMouseDown(EMouseButton::Right)
		&& InputState.CapturedViewIndex == ActiveViewIndex;
	FCameraMoveInput RawInput{};
	if (bCameraCaptured)
	{
		const auto AxisValue = [](EKeyCode Positive, EKeyCode Negative)
		{
			return (FInputSystem::IsKeyDown(Positive) ? 1.0f : 0.0f)
				- (FInputSystem::IsKeyDown(Negative) ? 1.0f : 0.0f);
		};
		RawInput.MoveAxis = {
			AxisValue(EKeyCode::W, EKeyCode::S),
			AxisValue(EKeyCode::D, EKeyCode::A),
			AxisValue(EKeyCode::E, EKeyCode::Q)
		};
		RawInput.MouseDelta = {
			static_cast<float>(FInputSystem::GetMouseDeltaX()),
			static_cast<float>(FInputSystem::GetMouseDeltaY())
		};
	}

	// 이미 한 번 읽은 휠 값을 대상 View에만 전달한다.
	RawInput.ZoomDelta = bCameraCaptured || PointerViewIndex == ActiveViewIndex
		? static_cast<float>(WheelDelta) : 0.0f;
	FViewport& Target = Viewports[ActiveViewIndex];
	Target.GetClient().UpdateCamera(DeltaTime, RawInput, Target.GetRect(), MoveSpeed, MouseSensitivity, bCameraCaptured);
}

bool FViewportsPanel::TryGetActiveViewRay(const FVector2 LocalMousePosition, FRay& OutRay) const
{
	if (!IsViewActive(ActiveViewIndex))
		return false;
	// Splitter gutter나 다른 View 위의 좌표를 이전 Active View의 Ray로 잘못 해석하지 않는다.
	if (Layout.FindViewAt(LocalMousePosition) != ActiveViewIndex)
		return false;
	const FViewport& Viewport = Viewports[ActiveViewIndex];
	OutRay = Viewport.Deproject(Viewport.ToViewLocal(LocalMousePosition));
	return true;
}

void FViewportsPanel::ReleaseRenderTargets()
{
	for (FViewport& Viewport : Viewports)
		Viewport.ReleaseRenderTargets();
}

// 마우스 위치에서 패널 원점을 빼 로컬 좌표로 바꾼다.
FVector2 FViewportsPanel::GetLocalMousePosition() const
{
	const ImVec2 Mouse = ImGui::GetMousePos();
	return {Mouse.x - ContentOrigin.x, Mouse.y - ContentOrigin.y};
}

// 엔진 입력 처리는 ImGui NewFrame보다 먼저 실행되므로 현재 OS 커서로 버튼 영역을 검사한다.
bool FViewportsPanel::IsHovered() const
{
	if (bStatResetButtonVisible && FStatOverlay::IsAnyEnabled())
	{
		POINT Cursor{};
		if (::GetCursorPos(&Cursor) &&
			Cursor.x >= StatResetButtonMin.x && Cursor.x < StatResetButtonMax.x &&
			Cursor.y >= StatResetButtonMin.y && Cursor.y < StatResetButtonMax.y)
			return false;
	}
	return bHovered;
}

float FViewportsPanel::ConsumeHorizontalDrag()
{
	const float Result = PendingHorizontalDrag;
	PendingHorizontalDrag = 0.0f;
	return Result;
}

float FViewportsPanel::ConsumeVerticalDrag()
{
	const float Result = PendingVerticalDrag;
	PendingVerticalDrag = 0.0f;
	return Result;
}

bool FViewportsPanel::ConsumeLayoutRequest(ELayoutMode& OutMode, int32& OutSingleViewIndex)
{
	if (!bHasLayoutRequest)
		return false;
	OutMode = RequestedLayoutMode;
	OutSingleViewIndex = RequestedSingleViewIndex;
	bHasLayoutRequest = false;
	return true;
}

bool FViewportsPanel::ConsumeCameraPresetRequest(int32& OutViewIndex, EMultipleViewportsCameraPreset& OutPreset)
{
	if (PendingCameraPresetViewIndex == InvalidViewIndex)
		return false;
	OutViewIndex = PendingCameraPresetViewIndex;
	OutPreset = PendingCameraPreset;
	PendingCameraPresetViewIndex = InvalidViewIndex;
	return true;
}

void FViewportsPanel::OnRender()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
	const bool bVisible = ImGui::Begin("Viewports", nullptr,
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse);
	if (!bVisible)
	{
		bHovered = false;
		bStatResetButtonVisible = false;
		ImGui::End();
		ImGui::PopStyleVar();
		return;
	}

	ContentOrigin = ImGui::GetCursorScreenPos();
	ContentSize = ImGui::GetContentRegionAvail();
	ContentSize.x = std::max(1.0f, ContentSize.x);
	ContentSize.y = std::max(1.0f, ContentSize.y);
	bHovered = ImGui::IsWindowHovered();
	bStatResetButtonVisible = false;

	// 전체 캔버스를 한 번 확보한 뒤 각 렌더 타깃을 창 DrawList에 직접 그린다.
	// Image 항목 네 개를 따로 배치하면 ImGui 레이아웃과 클리핑 상태가 삽입 순서에
	// 영향을 받아, Core가 올바른 사각형을 줘도 아래쪽 행이 잘릴 수 있다.
	ImGui::Dummy(ContentSize);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->PushClipRect(ContentOrigin,
		{ContentOrigin.x + ContentSize.x, ContentOrigin.y + ContentSize.y}, true);
	for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex)
	{
		const FViewport& Slot = Viewports[ViewIndex];
		if (!Slot.IsActive() || !Slot.GetColorTarget())
			continue;

		const ImVec2 ViewMin{ContentOrigin.x + Slot.GetRect().X, ContentOrigin.y + Slot.GetRect().Y};
		const ImVec2 ViewMax{ViewMin.x + Slot.GetRect().Width, ViewMin.y + Slot.GetRect().Height};
		DrawList->AddImage(Slot.GetColorTarget()->GetSRV(), ViewMin, ViewMax);
	}
	DrawList->PopClipRect();

	if (Viewports[1].IsActive() || Viewports[2].IsActive() || Viewports[3].IsActive())
	{
		// View Rect 사이에 비워 둔 gutter의 중앙에 Splitter 버튼을 배치한다.
		const float SplitX = (Viewports[0].GetRect().X + Viewports[0].GetRect().Width + Viewports[1].GetRect().X) * 0.5f;
		const float SplitY = (Viewports[0].GetRect().Y + Viewports[0].GetRect().Height + Viewports[2].GetRect().Y) * 0.5f;

		const ImVec2 VerticalMin{ContentOrigin.x + SplitX - SplitterThickness * 0.5f, ContentOrigin.y};
		const ImVec2 VerticalMax{VerticalMin.x + SplitterThickness, ContentOrigin.y + ContentSize.y};
		const ImVec2 HorizontalMin{ContentOrigin.x, ContentOrigin.y + SplitY - SplitterThickness * 0.5f};
		const ImVec2 HorizontalMax{ContentOrigin.x + ContentSize.x, HorizontalMin.y + SplitterThickness};

		ImGui::SetCursorScreenPos(VerticalMin);
		ImGui::InvisibleButton("##MultipleViewportsHorizontalSplitter", {SplitterThickness, ContentSize.y});
		ImGui::SetCursorScreenPos(HorizontalMin);
		ImGui::InvisibleButton("##MultipleViewportsVerticalSplitter", {ContentSize.x, SplitterThickness});

		const bool bVerticalHovered = bHovered && ImGui::IsMouseHoveringRect(VerticalMin, VerticalMax);
		const bool bHorizontalHovered = bHovered && ImGui::IsMouseHoveringRect(HorizontalMin, HorizontalMax);
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			bDraggingVerticalSplitter = bVerticalHovered;
			bDraggingHorizontalSplitter = bHorizontalHovered;
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			bDraggingVerticalSplitter = false;
			bDraggingHorizontalSplitter = false;
		}

		const bool bVerticalHighlighted = bDraggingVerticalSplitter || bVerticalHovered;
		const bool bHorizontalHighlighted = bDraggingHorizontalSplitter || bHorizontalHovered;
		DrawList->AddRectFilled(VerticalMin, VerticalMax,
			bVerticalHighlighted ? SplitterHoverColor : SplitterColor);
		DrawList->AddRectFilled(HorizontalMin, HorizontalMax,
			bHorizontalHighlighted ? SplitterHoverColor : SplitterColor);

		if (bDraggingVerticalSplitter)
			PendingHorizontalDrag += ImGui::GetIO().MouseDelta.x;
		if (bDraggingHorizontalSplitter)
			PendingVerticalDrag += ImGui::GetIO().MouseDelta.y;

		if (bVerticalHighlighted && bHorizontalHighlighted)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
		else if (bVerticalHighlighted)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		else if (bHorizontalHighlighted)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	}
	else
	{
		bDraggingVerticalSplitter = false;
		bDraggingHorizontalSplitter = false;
	}

	// 뷰포트별 반응형 툴바: 현재 줄에 공간이 없으면 다음 줄로 내려 배치한다.
	float ToolbarExtraHeight[4]{};

	for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex)
	{
		if (!Viewports[ViewIndex].IsActive())
			continue;

		const FViewport& Slot = Viewports[ViewIndex];

		const float Left = ContentOrigin.x + Slot.GetRect().X + 8.0f;
		const float Top = ContentOrigin.y + Slot.GetRect().Y + 8.0f;
		const float Width = std::max(1.0f, Slot.GetRect().Width - 16.0f);

		ImVec2 Cursor{ Left, Top };

		auto PlaceToolbarItem = [&](float PreferredWidth)
		{
			const float ItemWidth = std::min(PreferredWidth, Width);

			if (Cursor.x > Left && Cursor.x + ItemWidth > Left + Width)
			{
				Cursor.x = Left;
				Cursor.y += ImGui::GetFrameHeightWithSpacing();
			}

			ImGui::SetCursorScreenPos(Cursor);
			Cursor.x += ItemWidth + ImGui::GetStyle().ItemSpacing.x;

			return ItemWidth;
		};

		auto ComboWidth = [](const char* LongestLabel)
		{
			return ImGui::CalcTextSize(LongestLabel).x + ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2.0f;
		};

		auto SmallButtonWidth = [](const char* Label)
		{
			return ImGui::CalcTextSize(Label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
		};

		ImGui::PushID(100 + ViewIndex);

		// Camera Preset
		int SelectedPreset = static_cast<int>(GetCameraPreset(ViewIndex));
		ImGui::SetNextItemWidth(PlaceToolbarItem(ComboWidth("Ortho (Current)")));
		if (ImGui::Combo("##CameraPreset", &SelectedPreset, CameraPresetLabels, IM_ARRAYSIZE(CameraPresetLabels)))
		{
			PendingCameraPresetViewIndex = ViewIndex;
			PendingCameraPreset = static_cast<EMultipleViewportsCameraPreset>(SelectedPreset);
		}

		{
			FEngineShowFlags Flags = Viewports[ViewIndex].GetClient().GetShowFlags();
			ImGui::SetNextItemWidth(PlaceToolbarItem(ComboWidth("Show Flags")));

			if (ImGui::BeginCombo("##ViewShowFlags", "Show Flags"))
			{
				if (ShowFlagUI::DrawFlags(Flags, EShowFlagScope::View, EShowFlagGroup::Viewport))
				{
					Viewports[ViewIndex].GetClient().SetShowFlags(Flags);
				}
				ImGui::EndCombo();
			}

			// 뷰모드. 고른 값은 렌더 직전에 Renderer로 넘어가 UberLit 변형을 고른다.
			const EViewMode CurrentViewMode = Viewports[ViewIndex].GetClient().GetViewMode();

			// 가장 긴 뷰모드 이름 기준으로 Combo 폭을 잡는다.
			const char* LongestViewModeName = GetViewModeName(CurrentViewMode);
			for (int32 ModeIndex = 0; ModeIndex < static_cast<int32>(EViewMode::Count); ++ModeIndex)
			{
				const char* Name = GetViewModeName(static_cast<EViewMode>(ModeIndex));
				if (ImGui::CalcTextSize(Name).x > ImGui::CalcTextSize(LongestViewModeName).x)
					LongestViewModeName = Name;
			}

			ImGui::SetNextItemWidth(PlaceToolbarItem(ComboWidth(LongestViewModeName)));
			if (ImGui::BeginCombo("##ViewMode", GetViewModeName(CurrentViewMode)))
			{
				for (int32 ModeIndex = 0; ModeIndex < static_cast<int32>(EViewMode::Count); ++ModeIndex)
				{
					const EViewMode Candidate = static_cast<EViewMode>(ModeIndex);
					const bool bSelected = Candidate == CurrentViewMode;
					if (ImGui::Selectable(GetViewModeName(Candidate), bSelected))
						Viewports[ViewIndex].GetClient().SetViewMode(Candidate);
					if (bSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}

		// Single / Quad
		if (Layout.GetLayoutMode() == ELayoutMode::QuadSplit)
		{
			PlaceToolbarItem(SmallButtonWidth("Single"));
			if (ImGui::SmallButton("Single"))
			{
				RequestedLayoutMode = ELayoutMode::Single;
				RequestedSingleViewIndex = ViewIndex;
				bHasLayoutRequest = true;
			}
		}
		else if (ViewIndex == Layout.GetSingleViewIndex())
		{
			PlaceToolbarItem(SmallButtonWidth("Quad"));
			if (ImGui::SmallButton("Quad"))
			{
				RequestedLayoutMode = ELayoutMode::QuadSplit;
				RequestedSingleViewIndex = ViewIndex;
				bHasLayoutRequest = true;
			}
		}

		// Reset Stats
		if (ViewIndex == GetEditorViewIndex() &&
			FStatOverlay::IsAnyEnabled() &&
			(FStatOverlay::IsEnabled(EStatFlags::Profile) ||
				FStatRegistry::Find(EditorStats::STAT_PickingTime)))
		{
			PlaceToolbarItem(SmallButtonWidth("Reset Stats"));
			if (ImGui::SmallButton("Reset Stats"))
				FStatRegistry::Reset();
			StatResetButtonMin = ImGui::GetItemRectMin();
			StatResetButtonMax = ImGui::GetItemRectMax();
			bStatResetButtonVisible = true;
		}

		// 줄바꿈으로 늘어난 툴바 높이만큼 통계 오버레이를 아래로 민다.
		ToolbarExtraHeight[ViewIndex] = Cursor.y - Top;

		ImGui::PopID();
	}

	// 마지막으로 선택된 뷰포트만 오버레이
	for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex)
	{
		if (ViewIndex != GetEditorViewIndex())
			continue;
		if (!Viewports[ViewIndex].IsActive())
			continue;
		DrawStatOverlay(DrawList, {
			ContentOrigin.x + Viewports[ViewIndex].GetRect().X,
			ContentOrigin.y + Viewports[ViewIndex].GetRect().Y + ToolbarExtraHeight[ViewIndex]});
	}

	ImGui::End();
	ImGui::PopStyleVar();
}

// 항목별 줄을 모아 한 번에 배경과 텍스트를 그린다.
void FViewportsPanel::DrawStatOverlay(ImDrawList* DrawList, const ImVec2& ViewMin) const
{
	if (!DrawList || !FStatOverlay::IsAnyEnabled())
		return;

	constexpr float BytesPerMegabyte = 1024.0f * 1024.0f;

	// 제목은 UE처럼 노란색, 값은 흰색으로 구분한다.
	struct FStatLine
	{
		FString Text;
		ImU32 Color;
	};
	TArray<FStatLine> Lines;

	if (FStatOverlay::IsEnabled(EStatFlags::FPS))
	{
		Lines.Add({"FPS", TitleColor});
		Lines.Add({std::format("  {:.1f} fps", FStatOverlay::GetFPS()), ValueColor});
		Lines.Add({std::format("  {:.2f} ms", FStatOverlay::GetFrameTimeMs()), ValueColor});
	}

	if (FStatOverlay::IsEnabled(EStatFlags::Memory))
	{
		Lines.Add({"Memory", TitleColor});
		Lines.Add({std::format("  Object  {:.2f} MB ({} allocs)",
			static_cast<double>(FStatOverlay::GetObjectAllocationBytes()) / BytesPerMegabyte,
			FStatOverlay::GetObjectAllocationCount()), ValueColor});
		Lines.Add({std::format("  Process {:.2f} MB",
			static_cast<double>(FStatOverlay::GetProcessWorkingSetBytes()) / BytesPerMegabyte), ValueColor});
	}

	// Draw 수는 모든 View의 합이고 Fade로 건너뛴 Decal은 포함하지 않는다.
	if (FStatOverlay::IsEnabled(EStatFlags::Particles))
	{
		using namespace ParticleStats;
		Lines.Add({ "Particles / All Worlds (render work summed across views)", TitleColor });
		Lines.Add({ std::format("  Systems: {}  Emitters: {}  Active: {}  Capacity: {}",
			FStatRegistry::GetCounter(STAT_Systems), FStatRegistry::GetCounter(STAT_Emitters),
			FStatRegistry::GetCounter(STAT_ActiveParticles), FStatRegistry::GetCounter(STAT_Capacity)), ValueColor });
		Lines.Add({ std::format("  Visible Systems: {}  Submitted Emitters: {}  Draw Calls: {}",
			FStatRegistry::GetCounter(STAT_VisibleSystems), FStatRegistry::GetCounter(STAT_SubmittedEmitters),
			FStatRegistry::GetCounter(STAT_DrawCalls)), ValueColor });
		Lines.Add({ std::format("  Spawned: {}  Killed: {}  Spawn Dropped: {}",
			FStatRegistry::GetCounter(STAT_Spawned), FStatRegistry::GetCounter(STAT_Killed),
			FStatRegistry::GetCounter(STAT_SpawnDropped)), ValueColor });
		Lines.Add({ std::format("  Simulation: {:.3f} ms  Render Data: {:.3f} ms  Sort: {:.3f} ms",
			FStatRegistry::GetCurrentFrameMs(STAT_SimulationCPU), FStatRegistry::GetCurrentFrameMs(STAT_RenderDataCPU),
			FStatRegistry::GetCurrentFrameMs(STAT_SortCPU)), ValueColor });
		Lines.Add({ std::format("  Render Submit: {:.3f} ms (includes Upload: {:.3f} ms)",
			FStatRegistry::GetCurrentFrameMs(STAT_RenderSubmitCPU), FStatRegistry::GetCurrentFrameMs(STAT_UploadCPU)), ValueColor });
		Lines.Add({ std::format("  Sorted Particles: {}  Upload: {} bytes/frame",
			FStatRegistry::GetCounter(STAT_SortedParticles), FStatRegistry::GetCounter(STAT_UploadBytes)), ValueColor });
		Lines.Add({ std::format("  CPU Particle Storage: {:.3f} MB  GPU Buffers: {:.3f} MB",
			double(FStatRegistry::GetMemory(STAT_CPUMemory)) / BytesPerMegabyte,
			double(FStatRegistry::GetMemory(STAT_GPUBufferMemory)) / BytesPerMegabyte), ValueColor });
	}

	if (FStatOverlay::IsEnabled(EStatFlags::Decal))
	{
		Lines.Add({ "Decal", TitleColor });
		Lines.Add({ std::format("  CPU Pass: {:.3f} ms",
			FStatRegistry::GetCurrentFrameMs(DecalStats::STAT_RenderCPU)), ValueColor });
		Lines.Add({ std::format("  Total: {}  Visible: {}  Draws: {}  Additive: {}",
			FStatRegistry::GetCounter(DecalStats::STAT_Total),
			FStatRegistry::GetCounter(DecalStats::STAT_Visible),
			FStatRegistry::GetCounter(DecalStats::STAT_DrawCalls),
			FStatRegistry::GetCounter(DecalStats::STAT_AdditiveDrawCalls)), ValueColor });
		Lines.Add({ std::format("  Projected Build: {:.3f} ms  Builds: {}  Vertices: {}",
			FStatRegistry::GetCurrentFrameMs(DecalStats::STAT_ProjectedBuildCPU),
			FStatRegistry::GetCounter(DecalStats::STAT_ProjectedBuilds),
			FStatRegistry::GetCounter(DecalStats::STAT_ProjectedVertices)), ValueColor });
	}

	// CPU / GPU 스키닝 비교. 시간은 이번 프레임 값과 최근 120프레임 평균을 같이 보여 준다.
	if (FStatOverlay::IsEnabled(EStatFlags::Skinning))
	{
		auto AverageMs = [](TStatId Stat)
			{
				const FCycleStatData* Data = FStatRegistry::Find(Stat);
				return Data ? Data->GetRecentAverageMs() : 0.0;
			};
		auto TimeLine = [&](const char* Label, TStatId Stat)
			{
				return std::format("  {:<14} {:7.3f} ms  (avg {:7.3f})", Label,
					FStatRegistry::GetCurrentFrameMs(Stat), AverageMs(Stat));
			};

		Lines.Add({ std::format("Skinning / Mode: {}{}", FSkinningSettings::IsGPU() ? "GPU" : "CPU",
			FSkinningSettings::IsForceEveryFrame() ? "  (force every frame)" : ""), TitleColor });
		Lines.Add({ TimeLine("Anim Evaluate", SkinningStats::STAT_AnimEvaluate), ValueColor });
		Lines.Add({ TimeLine("Skinning", SkinningStats::STAT_Skin), ValueColor });
		Lines.Add({ TimeLine("Upload", SkinningStats::STAT_Upload), ValueColor });
		Lines.Add({ std::format("  {:<14} {:7.3f} ms  ({:.1f} fps)", "Frame",
			FStatOverlay::GetFrameTimeMs(), FStatOverlay::GetFPS()), ValueColor });
	}

	// 현재 프레임 전체 World의 그림자 통계를 기존 줄 목록에 추가한다.
	if (FStatOverlay::IsEnabled(EStatFlags::Shadow))
	{
		Lines.Add({ "Shadow / Total", TitleColor });
		// 전체 CPU 시간에는 그룹별 작업 외의 Atlas 준비와 정보 업로드 비용도 포함된다.
		Lines.Add({ std::format("  CPU Setup: {:.3f} ms  Renderer: {:.3f} ms",
			FStatRegistry::GetCurrentFrameMs(ShadowStats::STAT_SetupCPU),
			FStatRegistry::GetCurrentFrameMs(ShadowStats::STAT_RenderCPU)), ValueColor });
		Lines.Add({ std::format("  Prepared lights D/P/S: {}/{}/{}  Bound SRVs: {}",
			FStatRegistry::GetCounter(ShadowStats::STAT_DirectionalLights),
			FStatRegistry::GetCounter(ShadowStats::STAT_PointLights),
			FStatRegistry::GetCounter(ShadowStats::STAT_SpotLights),
			FStatRegistry::GetCounter(ShadowStats::STAT_BoundTextures)), ValueColor });
		Lines.Add({ std::format("  Local lights outside views: {}",
			FStatRegistry::GetCounter(ShadowStats::STAT_LocalLightsOutsideViews)), ValueColor });
		Lines.Add({ std::format("  Local lights without receivers: {}",
			FStatRegistry::GetCounter(ShadowStats::STAT_LocalLightsWithoutReceivers)), ValueColor });
		Lines.Add({ std::format("  Empty caster depth skips: {}",
			FStatRegistry::GetCounter(ShadowStats::STAT_EmptyCasterDepthSkips)), ValueColor });
		Lines.Add({ std::format("  Small caster submission skips: {}",
			FStatRegistry::GetCounter(ShadowStats::STAT_SmallCasterSkips)), ValueColor });
		Lines.Add({ std::format("  Caster sphere skips: {}",
			FStatRegistry::GetCounter(ShadowStats::STAT_CasterSphereSkips)), ValueColor });
		Lines.Add({ std::format("  Receiver projection skips: {}",
			FStatRegistry::GetCounter(ShadowStats::STAT_ReceiverProjectionSkips)), ValueColor });
		Lines.Add({ std::format("  Maps: {}  Draws: {}  Updates: {}",
			FStatRegistry::GetCounter(ShadowStats::STAT_Maps),
			FStatRegistry::GetCounter(ShadowStats::STAT_DrawCalls),
			FStatRegistry::GetCounter(ShadowStats::STAT_MapUpdates)), ValueColor });
		// CSM OFF와 직교 View의 단일 맵도 실제 Directional 투영에 포함한다.
		Lines.Add({ std::format("  Directional views: {}  Projections: {}",
			FStatRegistry::GetCounter(ShadowStats::STAT_DirectionalViews),
			FStatRegistry::GetCounter(ShadowStats::STAT_DirectionalProjections)), ValueColor });
		
		// Directional은 View별로 구분하고, Point와 Spot은 World 공유 그룹으로 표시한다.
		for (uint32 Group = 0; Group < ShadowStats::GroupCount; ++Group)
		{
			if (Group == ShadowStats::Local)
				Lines.Add({ "Local / World shared", TitleColor });
			else
			{
				const TStatId ViewStat = Group == ShadowStats::Single
					? ShadowStats::STAT_SingleViews : ShadowStats::STAT_CSMViews;
				Lines.Add({ std::format("{} / Views: {}", ShadowStats::GroupNames[Group],
					FStatRegistry::GetCounter(ViewStat)), TitleColor });
			}

			Lines.Add({ std::format("  CPU Setup: {:.3f} ms  Depth submit: {:.3f} ms",
				FStatRegistry::GetCurrentFrameMs(ShadowStats::SetupCPUStats[Group]),
				FStatRegistry::GetCurrentFrameMs(ShadowStats::DepthCPUStats[Group])), ValueColor });
			// 활성 타일 내용량은 아래의 전체 Atlas 페이지 할당량과 별개다.
			const double TileMiB = static_cast<double>(
				FStatRegistry::GetMemory(ShadowStats::TileMemoryStats[Group])) / BytesPerMegabyte;
			Lines.Add({ std::format("  Maps: {}  Draws: {}  Updates: {}  Tiles: {:.2f} MiB",
				FStatRegistry::GetCounter(ShadowStats::MapStats[Group]),
				FStatRegistry::GetCounter(ShadowStats::DrawStats[Group]),
				FStatRegistry::GetCounter(ShadowStats::UpdateStats[Group]),
				TileMiB), ValueColor });
		}

		Lines.Add({ "Shadow / Cache", TitleColor });
		Lines.Add({ std::format("  r.shadow.CacheWholeSceneShadows: {}  Generation: {}",
			FShadowSettings::IsWholeSceneCacheEnabled() ? "ON" : "OFF",
			FShadowSettings::GetCacheGeneration()), ValueColor });

		const uint64 CacheHits = FStatRegistry::GetCounter(ShadowStats::STAT_CacheHits);
		const uint64 MapUpdates = FStatRegistry::GetCounter(ShadowStats::STAT_MapUpdates);
		const uint64 CacheTotal = CacheHits + MapUpdates;
		const double HitRate = CacheTotal > 0 ? 100.0 * static_cast<double>(CacheHits) / static_cast<double>(CacheTotal) : 0.0;
		Lines.Add({ std::format("  Cached: {}  Updated: {}  Hit rate: {:.1f}%",
			CacheHits, MapUpdates, HitRate), ValueColor });
		// Point는 라이트 수가 아니라 캐시를 재사용한 면 수로 집계한다.
		Lines.Add({ std::format("  Cached D/P/S: {} / {} / {}",
			FStatRegistry::GetCounter(ShadowStats::STAT_CacheHitsDirectional),
			FStatRegistry::GetCounter(ShadowStats::STAT_CacheHitsPoint),
			FStatRegistry::GetCounter(ShadowStats::STAT_CacheHitsSpot)), ValueColor });
		Lines.Add({ std::format("  Miss Off / New / Gen / VP / Caster: {} / {} / {} / {} / {}",
			FStatRegistry::GetCounter(ShadowStats::STAT_CacheMissDisabled),
			FStatRegistry::GetCounter(ShadowStats::STAT_CacheMissNewTile),
			FStatRegistry::GetCounter(ShadowStats::STAT_CacheMissGeneration),
			FStatRegistry::GetCounter(ShadowStats::STAT_CacheMissViewProjection),
			FStatRegistry::GetCounter(ShadowStats::STAT_CacheMissCasters)), ValueColor });
		// 실제 확보한 페이지 용량 대비 POT 예약 블록의 점유율을 표시한다.
		const uint64 AtlasDepthBytes = FStatRegistry::GetMemory(ShadowStats::STAT_DepthMemory);
		const uint64 ReservedBytes = FStatRegistry::GetMemory(ShadowStats::STAT_AtlasReservedMemory);
		const double Occupancy = AtlasDepthBytes > 0
			? 100.0 * static_cast<double>(ReservedBytes) / static_cast<double>(AtlasDepthBytes)
			: 0.0;
		Lines.Add({ std::format("  Tile clears: {}  Atlas pages: {}  Allocations: {}",
			FStatRegistry::GetCounter(ShadowStats::STAT_TileClears),
			FStatRegistry::GetCounter(ShadowStats::STAT_AtlasPages),
			FStatRegistry::GetCounter(ShadowStats::STAT_AtlasAllocations)), ValueColor });
		Lines.Add({ std::format("  Reserved: {:.2f} MiB  Block occupancy: {:.1f}%",
			static_cast<double>(ReservedBytes) / BytesPerMegabyte, Occupancy), ValueColor });

		const uint64 DepthBytes = FStatRegistry::GetMemory(ShadowStats::STAT_DepthMemory);
		const uint64 ParameterBytes = FStatRegistry::GetMemory(ShadowStats::STAT_ParameterMemory);
		// 빈 타일과 재사용 대기 페이지를 포함해 실제 확보한 자원 크기를 표시한다.
		Lines.Add({ "Shadow / Allocated resources", TitleColor });
		Lines.Add({ std::format("  Depth: {:.2f} MiB  Info: {} B  Total: {:.2f} MiB",
			static_cast<double>(DepthBytes) / BytesPerMegabyte, ParameterBytes,
			static_cast<double>(DepthBytes + ParameterBytes) / BytesPerMegabyte), ValueColor });
	}

	struct FProfileStatLine
	{
		FString Text;
		ImU32 Color;
		FString Values[3];
	};
	TArray<FProfileStatLine> ProfileLines;
	if (FStatOverlay::IsEnabled(EStatFlags::Profile))
	{
		ProfileLines.Add({"CPU Profile (ms)", TitleColor, {"Last", "Avg", "Max"}});

		for (const auto& [Name, Data] : FStatRegistry::GetAll())
		{
			if (TStatId{ Name } == EditorStats::STAT_PickingTime)
				continue;

			FProfileStatLine Line{"  " + FString(Name), ValueColor};
			Line.Values[0] = std::format("{:.2f}", Data.GetLastMs());
			Line.Values[1] = std::format("{:.2f}", Data.GetRecentAverageMs());
			Line.Values[2] = std::format("{:.2f}", Data.GetMaxMs());
			ProfileLines.Add(Line);
		}
	}

	if (const FCycleStatData* PickingData = FStatRegistry::Find(EditorStats::STAT_PickingTime))
	{
		ProfileLines.Add({ "Picking", TitleColor });
		FProfileStatLine Line{"  Time (ms)", ValueColor};
		Line.Values[0] = std::format("Last: {:.2f}", PickingData->GetLastMs());
		Line.Values[1] = std::format("Attempts: {}", PickingData->CallCount);
		Line.Values[2] = std::format("Acc.: {:.2f}", PickingData->GetTotalMs());
		ProfileLines.Add(Line);
	}

	if (Lines.Num() == 0 && ProfileLines.Num() == 0)
		return;

	// 제어 위젯 한 줄 아래에서 시작해 Combo와 겹치지 않게 한다.
	const float LineHeight = ImGui::GetTextLineHeight();
	const ImVec2 Origin{
		ViewMin.x + StatOverlayMargin,
		ViewMin.y + StatOverlayMargin + ImGui::GetFrameHeight() + StatOverlayMargin};

	// 최대 너비 계산
	float MaxWidth = 0.0f;
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
		MaxWidth = std::max(MaxWidth, ImGui::CalcTextSize(Lines[Index].Text.c_str()).x);

	const float MinValueWidth = ImGui::CalcTextSize("00000.00").x;
	float NameWidth = 0.0f;
	float ValueWidths[3] = { MinValueWidth, MinValueWidth, MinValueWidth };
	bool bHasColumns = false;

	for (const FProfileStatLine& Line : ProfileLines)
	{
		const float TextWidth = ImGui::CalcTextSize(Line.Text.c_str()).x;
		bHasColumns = true;
		NameWidth = std::max(NameWidth, TextWidth);
		for (int32 Column = 0; Column < 3; ++Column)
			ValueWidths[Column] = std::max(MinValueWidth, ImGui::CalcTextSize(Line.Values[Column].c_str()).x);
	}

	float ColumnRight[3]{};
	if (bHasColumns)
	{
		float Width = NameWidth;
		for (int32 Column = 0; Column < 3; ++Column)
		{
			Width += ImGui::GetFontSize() + ValueWidths[Column];
			ColumnRight[Column] = Origin.x + Width;
		}
		MaxWidth = std::max(MaxWidth, Width);
	}

	const ImVec2 BackgroundMin{Origin.x - StatOverlayPadding, Origin.y - StatOverlayPadding};
	const ImVec2 BackgroundMax{
		Origin.x + MaxWidth + StatOverlayPadding,
		Origin.y + LineHeight * static_cast<float>(Lines.Num() + ProfileLines.Num()) + StatOverlayPadding};
	DrawList->AddRectFilled(BackgroundMin, BackgroundMax, StatOverlayBackgroundColor, 4.0f);

	// Lines 그리기
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		DrawList->AddText(
			{ Origin.x, Origin.y + LineHeight * static_cast<float>(Index) },
			Lines[Index].Color,
			Lines[Index].Text.c_str());
	}

	// ProfileLines 그리기
	for (int32 Index = 0; Index < ProfileLines.Num(); ++Index)
	{
		const FProfileStatLine& Line = ProfileLines[Index];
		const float Y = Origin.y + LineHeight * static_cast<float>(Lines.Num() + Index);
		DrawList->AddText({ Origin.x, Y }, Line.Color, Line.Text.c_str());

		for (int32 Column = 0; Column < 3; ++Column)
		{
			const FString& Text = Line.Values[Column];
			const float TextWidth = ImGui::CalcTextSize(Text.c_str()).x;
			DrawList->AddText({ ColumnRight[Column] - TextWidth, Y }, Line.Color, Text.c_str());
		}
	}
}

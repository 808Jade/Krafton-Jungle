#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/Viewports/Viewport.h"
#include "Editor/Viewports/ViewportTypes.h"
#include "Editor/LevelViewportLayout.h"

class UWorld;

// 레벨 에디터의 네 View. View 객체(FViewport) 넷과 Single/Quad 레이아웃, Splitter, 입력 Capture를 소유한다.
// (예전 FMultipleViewportsAdapter가 하던 일을 그대로 가져왔다. Week11의 FViewportsPanel과 같은 구조)
class FViewportsPanel : public IEditorPanel
{
public:
	static constexpr int32 ViewCount = FLevelViewportLayout::ViewCount;

	FViewportsPanel();

	bool Init() override;
	void Tick(float DeltaTime) override;
	// 각 View Texture와 Splitter·Layout·Preset 조작 UI를 ImGui 패널에 그린다.
	void OnRender() override;
	const char* GetPanelName() const override { return "Viewports"; }

	// --- View ---
	FViewport& GetViewport(int32 ViewIndex);
	const FViewport& GetViewport(int32 ViewIndex) const;
	// 지금 입력을 받는 View. 없거나 숨겨졌으면 nullptr이다.
	FViewport* GetActiveViewport();
	// 입력 영역 밖에서도 유지되는 마지막 편집 대상 View. Single에서는 확대한 View다.
	FViewport* GetEditorViewport();
	// 엔진 메인 카메라 설정을 읽고 네 View의 초기 카메라와 프리셋을 만든다.
	void InitializeFromWorld(UWorld& World);
	// 패널이 가진 View 중 그림자 View ID가 같은 것. 없으면 nullptr이다.
	FViewport* FindViewportByShadowId(uint32 ShadowViewId);

	// --- 레이아웃 (설정 패널·엔진이 쓴다) ---
	void SetLayoutMode(ELayoutMode Mode) { Layout.SetLayoutMode(Mode); }
	ELayoutMode GetLayoutMode() const { return Layout.GetLayoutMode(); }
	void SetSingleViewIndex(int32 ViewIndex) { Layout.SetSingleViewIndex(ViewIndex); }
	int32 GetSingleViewIndex() const { return Layout.GetSingleViewIndex(); }
	void SetSplitRatio(const FSplitRatio& Value) { Layout.SetSplitRatio(Value); }
	const FSplitRatio& GetSplitRatio() const { return Layout.GetSplitRatio(); }
	// 레이아웃을 갱신해 각 View의 영역·타깃을 맞추고, 숨겨진 View의 입력 상태를 보정한다.
	void UpdateLayout(FVector2 WindowSize);
	void ApplySplitterDrag(EDragAxis Axis, float DeltaPixels, FVector2 WindowSize)
	{
		Layout.ApplySplitterDrag(Axis, DeltaPixels, WindowSize);
	}
	bool IsViewActive(int32 ViewIndex) const { return Layout.IsViewActive(ViewIndex); }

	// --- 카메라 (설정 패널·Editor Controls가 쓴다) ---
	const FViewCamera& GetViewCamera(int32 ViewIndex) const { return GetViewport(ViewIndex).GetClient().GetCamera(); }
	void ApplyCameraProperties(int32 ViewIndex, const FViewCamera& Camera, bool bRotationChanged);
	void ApplyCameraPreset(int32 ViewIndex, EMultipleViewportsCameraPreset Preset);
	EMultipleViewportsCameraPreset GetCameraPreset(int32 ViewIndex) const { return GetViewport(ViewIndex).GetClient().GetCameraPreset(); }

	// --- 입력 ---
	// 우클릭 Capture View에 이동·회전·줌 입력을 적용한다. 프레임마다 한 번 부른다.
	void UpdateInput(float DeltaTime, FVector2 LocalMousePosition, float MoveSpeed, float MouseSensitivity);
	int32 GetActiveViewIndex() const { return ActiveViewIndex; }
	int32 GetEditorViewIndex() const { return Layout.GetLayoutMode() == ELayoutMode::Single ? Layout.GetSingleViewIndex() : EditorViewIndex; }
	void SetEditorViewIndex(int32 Index) { if (Index >= 0 && Index < ViewCount) EditorViewIndex = Index; }
	int32 GetCapturedViewIndex() const { return InputState.CapturedViewIndex; }
	// 활성 View 위의 로컬 마우스 좌표를 역투영한다. Splitter나 다른 View 위면 false다.
	bool TryGetActiveViewRay(FVector2 LocalMousePosition, FRay& OutRay) const;

	FVector2 GetContentSize() const { return {ContentSize.x, ContentSize.y}; }
	FVector2 GetLocalMousePosition() const;
	bool IsHovered() const;

	float ConsumeHorizontalDrag();
	float ConsumeVerticalDrag();
	// UI에서 발생한 Layout 변경 요청을 한 번 소비하도록 반환한다.
	bool ConsumeLayoutRequest(ELayoutMode& OutMode, int32& OutSingleViewIndex);
	// UI에서 발생한 View별 Camera Preset 요청을 한 번 소비하도록 반환한다.
	bool ConsumeCameraPresetRequest(int32& OutViewIndex, EMultipleViewportsCameraPreset& OutPreset);

	void ReleaseRenderTargets();

private:
	// 콘솔 stat 명령으로 켜진 항목을 해당 View의 좌상단에 겹쳐 그린다.
	void DrawStatOverlay(ImDrawList* DrawList, const ImVec2& ViewMin) const;

	// 소유(합성): 네 View. 그림자 View ID는 1~4다.
	FViewport Viewports[ViewCount];
	// 소유(합성): 화면 배치와 Splitter 비율
	FLevelViewportLayout Layout;
	FViewInputState InputState{};
	int32 ActiveViewIndex = InvalidViewIndex;
	int32 EditorViewIndex = 0;

	ImVec2 ContentOrigin{};
	ImVec2 ContentSize{1.0f, 1.0f};
	bool bHovered = false;
	bool bStatResetButtonVisible = false;
	ImVec2 StatResetButtonMin{};
	ImVec2 StatResetButtonMax{};
	float PendingHorizontalDrag = 0.0f;
	float PendingVerticalDrag = 0.0f;
	// 누른 뒤 커서가 Splitter 영역을 벗어나도 버튼을 놓을 때까지 드래그를 유지한다.
	bool bDraggingVerticalSplitter = false;
	bool bDraggingHorizontalSplitter = false;
	bool bHasLayoutRequest = false;
	ELayoutMode RequestedLayoutMode = ELayoutMode::QuadSplit;
	int32 RequestedSingleViewIndex = 0;
	int32 PendingCameraPresetViewIndex = InvalidViewIndex;
	EMultipleViewportsCameraPreset PendingCameraPreset = EMultipleViewportsCameraPreset::Perspective;
};

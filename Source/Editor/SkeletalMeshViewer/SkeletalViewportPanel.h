#pragma once
#include "Editor/EditorUI/EditorPanel.h"
#include "Core/EngineString.h"
#include "Containers/Array.h"

class FSkeletalPreviewTabBase;
struct FSkeletalBone;
class UAnimDataModel;
class UAnimNotify;

// 스켈레탈 메시 에디터 탭의 하위 패널: 프리뷰 뷰포트. 엔진이 탭의 프리뷰 월드를 그린 결과 텍스처를 띄우고, 본 이름 오버레이와 조작 안내를 그린다.
// 크기에 맞춰 탭의 FViewport 타깃을 맞추고, 표시·Hover 상태를 탭에 기록한다. (그리기는 엔진, 카메라 입력은 탭이 처리)
class FSkeletalViewportPanel : public IEditorPanel
{
public:
	// InTabId: 소속 탭의 ID. 같은 에디터 탭이 여러 개 열려도 ImGui 창 ID가 겹치지 않게 창 이름 뒤에 붙인다.
	FSkeletalViewportPanel(FSkeletalPreviewTabBase& InTab, const FString& InTabId, const ImGuiWindowClass& InWindowClass)
		: Tab(InTab), WindowClass(InWindowClass), PanelName(FString("Skeletal Mesh Viewport###SkelViewport_") + InTabId) {}

	bool Init() override { return true; }
	void Tick(float DeltaTime) override {}
	void OnRender() override;
	const char* GetPanelName() const override { return PanelName.c_str(); }

private:
	void DrawSkeletonOverlay(const ImVec2& ViewportOrigin, const ImVec2& ViewportSize);

	// 참조(집약): 이 패널을 소유한 탭. 편집 상태를 가진다. 탭이 패널보다 오래 산다.
	FSkeletalPreviewTabBase& Tab;
	// 참조(집약): 탭의 WindowClass. 이 패널은 그 탭의 DockSpace에만 도킹된다.
	const ImGuiWindowClass& WindowClass;
	FString PanelName;
};

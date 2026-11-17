#pragma once
#include "Editor/EditorUI/EditorTab.h"
#include <functional>

// 항상 첫 번째에 있고 닫을 수 없는 레벨 편집 탭.
// Viewports·Outliner·Details 같은 레벨 패널을 소유한다. 패널 포인터와 패널끼리의 연결은 엔진이 맡는다.
// 기존 imgui.ini 배치를 그대로 쓰도록 창 이름("Level Editor")·DockSpace 이름·WindowClass 이름을 예전과 같게 둔다.
class FLevelEditorTab : public FEditorTab
{
public:
	FLevelEditorTab();

	// 메뉴바(File/Window)와 PIE 툴바를 그리는 함수. 메뉴가 다른 탭·패널 목록을 알아야 하므로 FEditorUI가 넘겨준다.
	void SetHostContentsDrawer(std::function<void()> InDrawer) { HostContentsDrawer = std::move(InDrawer); }
	// 레벨 View 넷을 화면 목록으로 넘기는 함수. 어느 View가 PIE 월드를 그리는지 같은 판단은 엔진이 하므로 엔진이 넘겨준다.
	void SetViewCollector(std::function<void(TArray<FEditorTabView>&)> InCollector) { ViewCollector = std::move(InCollector); }

	// 레벨 View를 엔진의 공통 렌더 경로에 넘긴다. 탭이 가려졌으면 엔진이 부르지 않는다.
	void CollectViews(TArray<FEditorTabView>& OutViews) override;

protected:
	void DrawHostContents() override;
	ImGuiWindowFlags GetHostWindowFlags() const override;
	// 노드가 없거나 비었을 때, 또는 이전 크래시 ini로 Viewports가 도킹에서 떨어져 나왔을 때 다시 만든다.
	bool NeedsLayoutRebuild(const ImGuiDockNode* Node) const override;
	// 가운데 Viewports, 오른쪽 Outliner·Details, 아래쪽 Content Drawer·Output Log
	void BuildDefaultLayout(ImGuiID DockSpaceId) override;
	// Viewports는 레벨 탭에서 떼어 낼 수 없다. 나머지 패널은 탭 공통 클래스를 쓴다.
	const ImGuiWindowClass* GetPanelWindowClass(const IEditorPanel& Panel) const override;

private:
	std::function<void()> HostContentsDrawer;
	std::function<void(TArray<FEditorTabView>&)> ViewCollector;
	// 탭 공통 클래스 + 떼어 내기 금지
	ImGuiWindowClass ViewportWindowClass;
};

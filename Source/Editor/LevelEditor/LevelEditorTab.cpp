#include "EnginePCH.h"
#include "Editor/LevelEditor/LevelEditorTab.h"

#include <cstring>

namespace
{
	constexpr const char* ViewportsPanelName = "Viewports";

	FEditorTabSettings MakeLevelTabSettings()
	{
		FEditorTabSettings Settings;
		Settings.Title = "Level Editor";
		Settings.Id = "LevelEditor";
		Settings.bClosable = false;
		// 예전 DrawLevelEditorWorkspace와 같은 창·DockSpace·클래스 이름이라 기존 배치가 그대로 유지된다.
		Settings.bTitleIsWindowName = true;
		Settings.DockSpaceName = "LevelEditorInnerDockSpace";
		Settings.WindowClassName = "LevelEditorClass";
		// 루트 DockSpace를 처음 만들 때 이미 붙여 둔다. 강제로 옮기지 않는다.
		Settings.bDockOnFirstRender = false;
		// 다른 탭에 가려져도 패널이 자기 Hover 상태를 갱신해야 엔진이 레벨 뷰포트 입력을 바르게 거른다.
		Settings.bRenderPanelsWhenHidden = true;
		Settings.bZeroHostPadding = true;
		return Settings;
	}
}

FLevelEditorTab::FLevelEditorTab()
	: FEditorTab(MakeLevelTabSettings())
{
	ViewportWindowClass = GetWindowClass();
	ViewportWindowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoUndocking;
}

void FLevelEditorTab::CollectViews(TArray<FEditorTabView>& OutViews)
{
	if (ViewCollector)
		ViewCollector(OutViews);
}

void FLevelEditorTab::DrawHostContents()
{
	if (HostContentsDrawer)
		HostContentsDrawer();
}

ImGuiWindowFlags FLevelEditorTab::GetHostWindowFlags() const
{
	return ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
}

bool FLevelEditorTab::NeedsLayoutRebuild(const ImGuiDockNode* Node) const
{
	if (FEditorTab::NeedsLayoutRebuild(Node))
		return true;
	const ImGuiWindow* ViewportsWindow = ImGui::FindWindowByName(ViewportsPanelName);
	return ViewportsWindow != nullptr && ViewportsWindow->DockNode == nullptr && ViewportsWindow->WasActive;
}

void FLevelEditorTab::BuildDefaultLayout(const ImGuiID DockSpaceId)
{
	ImGuiID CenterNode = DockSpaceId;
	ImGuiID DownNode = ImGui::DockBuilderSplitNode(CenterNode, ImGuiDir_Down, 0.25f, nullptr, &CenterNode);
	ImGuiID RightNode = ImGui::DockBuilderSplitNode(CenterNode, ImGuiDir_Right, 0.25f, nullptr, &CenterNode);
	const ImGuiID RightBottomNode = ImGui::DockBuilderSplitNode(RightNode, ImGuiDir_Down, 0.65f, nullptr, &RightNode);
	const ImGuiID DownRightNode = ImGui::DockBuilderSplitNode(DownNode, ImGuiDir_Right, 0.40f, nullptr, &DownNode);

	ImGui::DockBuilderDockWindow(ViewportsPanelName, CenterNode);
	ImGui::DockBuilderDockWindow("Outliner", RightNode);
	ImGui::DockBuilderDockWindow("Details", RightBottomNode);
	ImGui::DockBuilderDockWindow("Editor Controls", RightNode);
	ImGui::DockBuilderDockWindow("Settings", RightNode);
	ImGui::DockBuilderDockWindow("Content Drawer", DownNode);
	ImGui::DockBuilderDockWindow("Sequence", DownNode);
	ImGui::DockBuilderDockWindow("Output Log", DownRightNode);
}

const ImGuiWindowClass* FLevelEditorTab::GetPanelWindowClass(const IEditorPanel& Panel) const
{
	return std::strcmp(Panel.GetPanelName(), ViewportsPanelName) == 0 ? &ViewportWindowClass : &GetWindowClass();
}

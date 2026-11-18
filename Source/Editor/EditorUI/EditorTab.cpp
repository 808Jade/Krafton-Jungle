#include "EnginePCH.h"
#include "Editor/EditorUI/EditorTab.h"

#include <algorithm>

FEditorTab::FEditorTab(FEditorTabSettings InSettings)
	: Settings(std::move(InSettings))
{
	if (Settings.DockSpaceName.empty())
		Settings.DockSpaceName = "TabDockSpace";
	if (Settings.WindowClassName.empty())
		Settings.WindowClassName = Settings.Id;
	bDockOnFirstRender = Settings.bDockOnFirstRender;

	// 탭마다 다른 클래스: 다른 탭(같은 종류의 에디터 포함)이나 레벨 에디터의 패널과 섞여 도킹되지 않는다.
	WindowClass.ClassId = ImHashStr(Settings.WindowClassName.c_str());
	WindowClass.DockingAllowUnclassed = false;
}

void FEditorTab::Tick(const float DeltaTime)
{
	for (auto& Panel : Panels)
		Panel->Tick(DeltaTime);
}

bool FEditorTab::NeedsLayoutRebuild(const ImGuiDockNode* Node) const
{
	return Node == nullptr || (Node->IsLeafNode() && Node->Windows.Size == 0);
}

void FEditorTab::Render(const ImGuiID DefaultDockId)
{
	OnPreRender();

	const FString WindowName = GetWindowName();

	ImGui::SetNextWindowSize(ImVec2(1280, 800), ImGuiCond_FirstUseEver);
	// 새로 연 탭은 항상 레벨 에디터 옆에 붙인다. imgui.ini에 같은 ID의 옛 배치가 남아 있으면
	// FirstUseEver가 무시되어 떠 있는 창으로 열리므로, 처음 그릴 때 한 번만 Always로 지정한다.
	// 그 뒤로는 사용자가 옮긴 위치를 그대로 둔다.
	if (DefaultDockId && bDockOnFirstRender)
		ImGui::SetNextWindowDockID(DefaultDockId, ImGuiCond_Always);
	bDockOnFirstRender = false;
	if (bFocusRequested)
	{
		ImGui::SetNextWindowFocus();
		bFocusRequested = false;
	}

	bool bStillOpen = bOpen;
	if (Settings.bZeroHostPadding)
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	bHostVisible = ImGui::Begin(WindowName.c_str(), Settings.bClosable ? &bStillOpen : nullptr, GetHostWindowFlags());
	if (Settings.bZeroHostPadding)
		ImGui::PopStyleVar();
	if (bHostVisible)
		DrawHostContents();

	// 내부 DockSpace의 ID는 호스트 창 ID 아래에서 만들어지므로 탭마다 다르다.
	const ImGuiID DockSpaceId = ImGui::GetID(Settings.DockSpaceName.c_str());
	const ImGuiDockNode* Node = ImGui::DockBuilderGetNode(DockSpaceId);
	if (bHostVisible && (bResetLayout || NeedsLayoutRebuild(Node)))
	{
		bResetLayout = false;
		const ImVec2 Available = ImGui::GetContentRegionAvail();
		const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
		ImGui::DockBuilderRemoveNode(DockSpaceId);
		ImGui::DockBuilderAddNode(DockSpaceId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(DockSpaceId, ImVec2(
			Available.x > 100.0f ? Available.x : std::max(MainViewport->WorkSize.x, 800.0f),
			Available.y > 100.0f ? Available.y : std::max(MainViewport->WorkSize.y, 600.0f)));
		BuildDefaultLayout(DockSpaceId);
		ImGui::DockBuilderFinish(DockSpaceId);
	}

	// 탭이 가려져도 DockSpace는 매 프레임 제출해야 하위 패널이 도킹에서 풀리지 않는다.
	const ImGuiDockNodeFlags DockFlags = (bHostVisible && bStillOpen) ? ImGuiDockNodeFlags_None : ImGuiDockNodeFlags_KeepAliveOnly;
	ImGui::DockSpace(DockSpaceId, ImVec2(0.0f, 0.0f), DockFlags, &WindowClass);
	ImGui::End();

	// 하위 패널은 자기가 어느 탭 안에 있는지 모른다. 도킹 클래스는 여기서 정해 준다.
	if (bStillOpen && (bHostVisible || Settings.bRenderPanelsWhenHidden))
	{
		for (auto& Panel : Panels)
		{
			if (!Panel->IsOpen())
				continue;
			ImGui::SetNextWindowClass(GetPanelWindowClass(*Panel));
			Panel->OnRender();
		}
	}

	bOpen = bStillOpen;
	if (!bOpen)
		bHostVisible = false;
}

void FEditorTab::BuildDefaultLayout(const ImGuiID DockSpaceId)
{
	for (auto& Panel : Panels)
		ImGui::DockBuilderDockWindow(Panel->GetPanelName(), DockSpaceId);
}

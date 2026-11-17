#include "EnginePCH.h"
#include "Editor/EditorUI/EditorUI.h"

#include "Editor/LevelEditor/LevelEditorTab.h"

#include "imgui.h"
#include "imgui_internal.h"

bool FEditorUI::Init(bool bInUseDockSpace, bool bInPassthruCentralNode)
{
	bUseDockSpace = bInUseDockSpace;
	bPassthruCentralNode = bInPassthruCentralNode;

	Toolbar = MakeUnique<FLevelEditorToolbar>();

	// 탭 0은 항상 레벨 탭이다. 메뉴바와 PIE 툴바는 다른 탭·패널 목록을 알아야 하므로 여기서 그려 준다.
	auto LevelTab = MakeUnique<FLevelEditorTab>();
	LevelTab->SetHostContentsDrawer([this]()
	{
		DrawMainMenuBar();
		if (Toolbar)
			Toolbar->Draw();
	});
	Tabs.Add(std::move(LevelTab));

	return true;
}

FLevelEditorTab* FEditorUI::GetLevelTab() const
{
	return static_cast<FLevelEditorTab*>(Tabs[0].get());
}

void FEditorUI::Tick(float DeltaTime)
{
	for (auto& Panel : Panels)
	{
		Panel->Tick(DeltaTime);
	}
	for (auto& Tab : Tabs)
	{
		Tab->Tick(DeltaTime);
	}
}

bool FEditorUI::IsAnyTabHoveredOrInteracting() const
{
	for (const auto& Tab : Tabs)
	{
		if (Tab->IsVisible() && Tab->IsHoveredOrInteracting())
			return true;
	}
	return false;
}

void FEditorUI::CollectTabViews(TArray<FEditorTabView>& OutViews) const
{
	for (const auto& Tab : Tabs)
	{
		if (Tab->IsVisible())
			Tab->CollectViews(OutViews);
	}
}

void FEditorUI::ReleaseTabResources()
{
	for (auto& Tab : Tabs)
		Tab->ReleaseResources();
	// 레벨 탭(닫을 수 없는 탭)은 남긴다. 그 패널을 엔진이 종료 끝까지 참조한다.
	for (int32 Index = Tabs.Num() - 1; Index >= 0; --Index)
	{
		if (Tabs[Index]->CanClose())
			Tabs.RemoveAt(Index);
	}
}

void FEditorUI::RenderTabs(const ImGuiID RootWorkspaceDockId)
{
	// 처음 뜨는 탭은 레벨 에디터와 같은 탭 묶음에 붙인다. 이후 위치는 imgui.ini가 기억한다.
	const ImGuiWindow* LevelWindow = ImGui::FindWindowByName("Level Editor");
	const ImGuiID DefaultDockId = LevelWindow && LevelWindow->DockId ? LevelWindow->DockId : RootWorkspaceDockId;

	// 레벨 탭(0)은 OnRender 앞부분에서 이미 그렸다.
	// 탭 안의 메뉴(File > Open 등)가 그리는 도중에 AddTab으로 새 탭을 붙일 수 있다.
	// 배열이 재할당되면 범위 기반 for의 반복자가 무효가 되므로 인덱스로 돈다.
	// 탭 객체 자체는 힙에 있어 재할당 뒤에도 그대로이고, 새로 붙은 탭은 같은 프레임에 이어서 그려진다.
	for (int32 Index = 1; Index < Tabs.Num(); ++Index)
		Tabs[Index]->Render(DefaultDockId);

	// 닫힌 탭은 그리기가 끝난 뒤에 지운다. 프리뷰 월드·GPU 타깃을 먼저 놓는다.
	for (int32 Index = Tabs.Num() - 1; Index >= 1; --Index)
	{
		if (Tabs[Index]->IsOpen())
			continue;
		Tabs[Index]->ReleaseResources();
		Tabs.RemoveAt(Index);
	}
}

void FEditorUI::OnRender()
{
	ImGuiID RootWorkspaceDockId = 0;
	if (bUseDockSpace)
	{
		static bool dockspaceOpen = true;
		static bool optFullscreen = true;
		static bool optPadding = false;
		static ImGuiDockNodeFlags dockspaceFlags = bPassthruCentralNode
			? ImGuiDockNodeFlags_PassthruCentralNode
			: ImGuiDockNodeFlags_None;

		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;
		if (optFullscreen)
		{
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}

		if (!optPadding)
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });

		ImGui::Begin("DockingSpace", &dockspaceOpen, windowFlags);
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(2);

		if (!optPadding)
			ImGui::PopStyleVar();

		if (optFullscreen)
			ImGui::PopStyleVar(2);

		// 최상위 루트 DockSpace: [Level Editor]와 [Skeletal Mesh Viewer]가 탭으로 도킹되는 공간
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID rootDockId = ImGui::GetID("HitoriEngineRootDockSpace");
			RootWorkspaceDockId = rootDockId;

			ImGuiDockNode* RootNode = ImGui::DockBuilderGetNode(rootDockId);
			if (RootNode == nullptr || (RootNode->CentralNode == nullptr && RootNode->IsLeafNode()))
			{
				const ImGuiViewport* viewport = ImGui::GetMainViewport();
				ImVec2 rootSize(std::max(viewport->WorkSize.x, 800.0f), std::max(viewport->WorkSize.y, 600.0f));
				ImGui::DockBuilderRemoveNode(rootDockId);
				ImGui::DockBuilderAddNode(rootDockId, ImGuiDockNodeFlags_DockSpace);
				ImGui::DockBuilderSetNodeSize(rootDockId, rootSize);

				ImGui::DockBuilderDockWindow("Level Editor", rootDockId);
				ImGui::DockBuilderDockWindow("Skeletal Mesh Viewer", rootDockId);
				ImGui::DockBuilderDockWindow("Blend Space Editor", rootDockId);
				ImGui::DockBuilderDockWindow("State Machine Editor", rootDockId);
                ImGui::DockBuilderDockWindow("AnimGraph Editor", rootDockId);
				ImGui::DockBuilderFinish(rootDockId);
			}

			ImGui::DockSpace(rootDockId, ImVec2(0.0f, 0.0f), dockspaceFlags);
		}

		ImGui::End();
	}

	// 1. 레벨 탭(탭 0). 레벨 패널은 이 탭이 소유하고 자기 클래스로 자기 DockSpace에만 붙인다.
	// 루트 DockSpace를 처음 만들 때 이미 붙여 두므로 기본 도킹 위치는 넘기지 않는다.
	GetLevelTab()->Render(0);

	// 아래 에셋 에디터가 레벨 탭 안에 잘못 붙었는지 판정할 때 쓴다. (레벨 탭의 WindowClass 이름과 같다)
	const ImGuiID LevelEditorClassId = ImHashStr("LevelEditorClass");

	// 2. 아직 탭으로 옮기지 않은 에셋 에디터 패널 (Skeletal Mesh Viewer, Blend Space, State Machine, AnimGraph)
	for (auto& Panel : Panels)
	{
		if (!Panel->IsOpen())
			continue;

		const char* Name = Panel->GetPanelName();
		const bool bIsAnimGraphEditor = (strcmp(Name, "AnimGraph Editor") == 0);

		if (strcmp(Name, "State Machine Editor") == 0 || bIsAnimGraphEditor)
		{
			// Asset editors belong to the outer workspace, not the locked Level Editor panels.
			ImGuiWindow* Window = ImGui::FindWindowByName(Name);
			ImGuiID SavedDockId = Window ? Window->DockId : 0;
			if (!Window)
				if (const ImGuiWindowSettings* Settings = ImGui::FindWindowSettingsByID(ImHashStr(Name)))
					SavedDockId = Settings->DockId;
			ImGuiDockNode* Node = ImGui::DockBuilderGetNode(SavedDockId);
			bool bWrongGroup = false;
			if (Node)
			{
				ImGuiDockNode* Root = ImGui::DockNodeGetRootNode(Node);
				bWrongGroup = (Node->MergedFlags & ImGuiDockNodeFlags_NoUndocking) != 0 ||
					Root->WindowClass.ClassId == LevelEditorClassId ||
					(Window && Window->WindowClass.ClassId == LevelEditorClassId);
			}
			if (bWrongGroup || (bIsAnimGraphEditor && bAnimGraphInitialDockPending && !Node))
			{
				if (bIsAnimGraphEditor && RootWorkspaceDockId)
				{
					const ImGuiWindow* LevelWindow = ImGui::FindWindowByName("Level Editor");
					const ImGuiID Target = LevelWindow && LevelWindow->DockId ? LevelWindow->DockId : RootWorkspaceDockId;
					ImGui::SetNextWindowDockID(Target, ImGuiCond_Always);
				}
				else
					ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
			}
			if (bIsAnimGraphEditor)
				bAnimGraphInitialDockPending = false;
			ImGuiWindowClass AssetEditorClass;
			ImGui::SetNextWindowClass(&AssetEditorClass);
		}

		Panel->OnRender();
	}

	// 3. 에셋 에디터 탭 (레벨 탭 제외)
	RenderTabs(RootWorkspaceDockId);
}

void FEditorUI::DrawMainMenuBar()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Scene")) { if (OnNewScene) OnNewScene(); }
			if (ImGui::MenuItem("Open Scene...")) { if (OnOpenScene) OnOpenScene(); }

			ImGui::Separator();

			if (ImGui::MenuItem("Save")) { if (OnSaveScene) OnSaveScene(); }
			if (ImGui::MenuItem("Save As...")) { if (OnSaveSceneAs) OnSaveSceneAs(); }

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window"))
		{
			// 레벨 탭의 패널, 그다음 아직 패널 방식인 에셋 에디터
			const auto DrawPanelToggle = [](IEditorPanel& Panel)
			{
				const bool bOpen = Panel.IsOpen();
				if (ImGui::MenuItem(Panel.GetPanelName(), nullptr, bOpen))
					Panel.SetOpen(!bOpen);
			};
			for (const auto& Panel : GetLevelTab()->GetPanels())
				DrawPanelToggle(*Panel);
			ImGui::Separator();
			for (auto& Panel : Panels)
				DrawPanelToggle(*Panel);

			// 열려 있는 에셋 에디터 탭(레벨 탭 제외). 누르면 그 탭을 앞으로 가져온다.
			if (Tabs.Num() > 1)
			{
				ImGui::Separator();
				for (int32 Index = 1; Index < Tabs.Num(); ++Index)
				{
					FEditorTab& Tab = *Tabs[Index];
					ImGui::PushID(Tab.GetId().c_str());
					if (ImGui::MenuItem(Tab.GetTitle().c_str()))
						Tab.RequestFocus();
					ImGui::PopID();
				}
			}

			if (!WindowMenuCommands.IsEmpty())
			{
				ImGui::Separator();
				for (const FWindowMenuCommand& Entry : WindowMenuCommands)
				{
					if (ImGui::MenuItem(Entry.Label.c_str()) && Entry.Command)
						Entry.Command();
				}
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Reset Level Editor Layout"))
			{
				GetLevelTab()->RequestLayoutReset();
			}

			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
}

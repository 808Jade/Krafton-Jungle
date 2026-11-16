#include "EnginePCH.h"
#include "Editor/ParticleSystem/ParticleSystemEditorPanel.h"
#include "Editor/ParticleSystem/ParticleSystemViewportPanel.h"
#include "Editor/ParticleSystem/ParticleSystemEmitterPanel.h"
#include "Editor/ParticleSystem/ParticleSystemDetailsPanel.h"

#include <algorithm>

namespace
{
	constexpr const char* InnerDockSpaceName = "ParticleSystemEditorInnerDockSpace";
	constexpr const char* WindowClassName = "ParticleSystemEditorClass";
}

FParticleSystemEditorPanel::FParticleSystemEditorPanel() = default;

FParticleSystemEditorPanel::~FParticleSystemEditorPanel()
{
	ReleaseResources();
}

bool FParticleSystemEditorPanel::Init()
{
	if (!Context.Init())
		return false;

	// 하위 패널은 Context를 참조만 한다. 생성 순서가 곧 그리는 순서다.
	auto Viewport = MakeUnique<FParticleSystemViewportPanel>(Context);
	ViewportPanel = Viewport.get();
	SubPanels.Add(std::move(Viewport));
	SubPanels.Add(MakeUnique<FParticleSystemEmitterPanel>(Context));
	SubPanels.Add(MakeUnique<FParticleSystemDetailsPanel>(Context));

	bool bAllInitialized = true;
	for (auto& Panel : SubPanels)
		bAllInitialized &= Panel->Init();
	return bAllInitialized;
}

void FParticleSystemEditorPanel::Tick(float DeltaTime)
{
	// 닫혀 있거나 다른 탭에 가려진 동안에는 시뮬레이션과 하위 패널을 진행하지 않는다.
	if (!IsOpen() || !bHostVisible)
		return;

	Context.Tick(DeltaTime);
	for (auto& Panel : SubPanels)
		Panel->Tick(DeltaTime);
}

void FParticleSystemEditorPanel::OnRender()
{
	bool bOpen = IsOpen();
	ImGui::SetNextWindowSize(ImVec2(1280, 800), ImGuiCond_FirstUseEver);
	bHostVisible = ImGui::Begin(GetPanelName(), &bOpen, ImGuiWindowFlags_MenuBar);

	if (bHostVisible)
	{
		DrawMenuBar();	// ①
		DrawToolBar();	// ②
	}

	// 에디터 전용 WindowClass. 하위 패널은 이 DockSpace에만 붙고, 레벨 에디터 창과 섞이지 않는다.
	ImGuiWindowClass EditorClass;
	EditorClass.ClassId = ImHashStr(WindowClassName);
	EditorClass.DockingAllowUnclassed = false;

	const ImGuiID DockSpaceId = ImGui::GetID(InnerDockSpaceName);
	const ImGuiDockNode* Node = ImGui::DockBuilderGetNode(DockSpaceId);
	if (Node == nullptr || bResetLayout || (Node->IsLeafNode() && Node->Windows.Size == 0))
	{
		bResetLayout = false;
		BuildDefaultLayout(DockSpaceId);
	}

	// 탭이 가려져도 DockSpace는 매 프레임 제출해야 하위 패널이 도킹에서 풀리지 않는다.
	const ImGuiDockNodeFlags DockFlags = (bHostVisible && bOpen) ? ImGuiDockNodeFlags_None : ImGuiDockNodeFlags_KeepAliveOnly;
	ImGui::DockSpace(DockSpaceId, ImVec2(0.0f, 0.0f), DockFlags, &EditorClass);
	ImGui::End();

	// ③④⑤ 하위 패널. 도킹 클래스는 여기서 정해 주므로 패널은 자기가 어느 에디터 안에 있는지 모른다.
	if (bHostVisible && bOpen)
	{
		for (auto& Panel : SubPanels)
		{
			if (!Panel->IsOpen())
				continue;
			ImGui::SetNextWindowClass(&EditorClass);
			Panel->OnRender();
		}
	}

	SetOpen(bOpen);
	if (!bOpen)
		bHostVisible = false;
}

void FParticleSystemEditorPanel::DrawMenuBar()
{
	if (!ImGui::BeginMenuBar())
		return;

	if (ImGui::BeginMenu("Window"))
	{
		for (auto& Panel : SubPanels)
		{
			// "Details##Particle"처럼 ## 뒤는 ID용이므로 메뉴에는 앞부분만 보인다.
			const bool bPanelOpen = Panel->IsOpen();
			if (ImGui::MenuItem(Panel->GetPanelName(), nullptr, bPanelOpen))
				Panel->SetOpen(!bPanelOpen);
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Reset Layout"))
		{
			for (auto& Panel : SubPanels)
				Panel->SetOpen(true);
			bResetLayout = true;
		}
		ImGui::EndMenu();
	}

	ImGui::EndMenuBar();
}

void FParticleSystemEditorPanel::DrawToolBar()
{
	if (ImGui::Button("Restart Sim"))
		Context.RestartSimulation();
	ImGui::SameLine();
	ImGui::Checkbox("Pause", &Context.bSimulationPaused);
	ImGui::SameLine();
	if (ImGui::Button("Reset Camera") && ViewportPanel)
		ViewportPanel->ResetCamera();
	ImGui::Separator();
}

void FParticleSystemEditorPanel::BuildDefaultLayout(const unsigned int DockSpaceId)
{
	// Cascade 배치: 왼쪽 위 뷰포트, 왼쪽 아래 디테일, 오른쪽 이미터
	const ImVec2 Available = ImGui::GetContentRegionAvail();
	const ImVec2 DockSize(std::max(Available.x, 800.0f), std::max(Available.y, 600.0f));

	ImGui::DockBuilderRemoveNode(DockSpaceId);
	ImGui::DockBuilderAddNode(DockSpaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(DockSpaceId, DockSize);

	ImGuiID LeftNode = DockSpaceId;
	const ImGuiID RightNode = ImGui::DockBuilderSplitNode(LeftNode, ImGuiDir_Right, 0.60f, nullptr, &LeftNode);
	const ImGuiID LeftBottomNode = ImGui::DockBuilderSplitNode(LeftNode, ImGuiDir_Down, 0.45f, nullptr, &LeftNode);

	for (auto& Panel : SubPanels)
	{
		const char* Name = Panel->GetPanelName();
		const ImGuiID Target = Panel.get() == ViewportPanel ? LeftNode
			: strcmp(Name, "Details##Particle") == 0 ? LeftBottomNode : RightNode;
		ImGui::DockBuilderDockWindow(Name, Target);
	}
	ImGui::DockBuilderFinish(DockSpaceId);
}

bool FParticleSystemEditorPanel::IsHoveredOrInteracting() const
{
	return IsOpen() && bHostVisible && ViewportPanel && ViewportPanel->IsOpen() && ViewportPanel->IsHoveredOrInteracting();
}

FParticleSystemViewportPanel* FParticleSystemEditorPanel::GetViewportPanel() const
{
	return IsOpen() && bHostVisible ? ViewportPanel : nullptr;
}

void FParticleSystemEditorPanel::ReleaseResources()
{
	// 타깃은 Device 종료 전에, 프리뷰 월드는 전역 UObject 일괄 삭제 전에 놓는다.
	if (ViewportPanel)
		ViewportPanel->ReleaseResources();
	Context.Release();
}

#include "EnginePCH.h"
#include "Editor/ParticleSystem/ParticleSystemEditorPanel.h"
#include "Editor/ParticleSystem/ParticleSystemViewportPanel.h"
#include "Editor/ParticleSystem/ParticleSystemEmitterPanel.h"
#include "Editor/ParticleSystem/ParticleSystemDetailsPanel.h"

#include "Editor/HitoriEd/EditorResourcePaths.h"
#include "Components/ParticleSystemComponent.h"
#include "Asset/AssetManager.h"
#include "Render/Texture2D.h"

#include <algorithm>

namespace
{
	constexpr const char* InnerDockSpaceName = "ParticleSystemEditorInnerDockSpace";
	constexpr const char* WindowClassName = "ParticleSystemEditorClass";

	constexpr float ToolbarIconSize = 18.0f;
	constexpr float ToolbarGroupSpacing = 10.0f;

	// 아직 파티클 에셋과 연결되지 않아 동작할 수 없는 버튼의 툴팁 끝에 붙인다.
	constexpr const char* NeedsAssetNote = "\n(Disabled: needs the particle system asset, not connected yet)";

	// 아이콘과 글자를 한 버튼 안에 그린다. 클릭 영역은 버튼 전체다. (언리얼 툴바 버튼과 같은 모양)
	// bEnabled가 false면 흐리게 그리고 클릭을 받지 않는다. 툴팁은 비활성일 때도 띄워 이유를 보여 준다.
	// bToggled가 true면 눌린 색으로 그려 켜진 토글임을 보여 준다. (원점 축·바운드 같은 표시 토글)
	bool IconTextButton(const char* Id, UTexture2D* Icon, const char* Label, const FString& Tooltip,
		bool bEnabled = true, bool bToggled = false)
	{
		ImGui::BeginDisabled(!bEnabled);
		if (bToggled)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		}
		const ImGuiStyle& Style = ImGui::GetStyle();
		const ImVec2 TextSize = ImGui::CalcTextSize(Label);
		const float IconWidth = Icon ? ToolbarIconSize + Style.ItemInnerSpacing.x : 0.0f;
		const float Height = std::max(ToolbarIconSize, TextSize.y) + Style.FramePadding.y * 2.0f;
		const ImVec2 Size(IconWidth + TextSize.x + Style.FramePadding.x * 2.0f, Height);

		const bool bClicked = ImGui::Button(Id, Size);
		const ImVec2 Min = ImGui::GetItemRectMin();
		ImDrawList* DrawList = ImGui::GetWindowDrawList();

		float X = Min.x + Style.FramePadding.x;
		if (Icon)
		{
			const float IconY = Min.y + (Height - ToolbarIconSize) * 0.5f;
			// GetColorU32가 현재 Style.Alpha를 곱하므로 비활성일 때 아이콘도 함께 흐려진다.
			DrawList->AddImage(reinterpret_cast<ImTextureID>(Icon->GetResource()->GetSRV()),
				ImVec2(X, IconY), ImVec2(X + ToolbarIconSize, IconY + ToolbarIconSize),
				ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(IM_COL32_WHITE));
			X += IconWidth;
		}
		DrawList->AddText(ImVec2(X, Min.y + (Height - TextSize.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), Label);
		if (bToggled)
			ImGui::PopStyleColor(2);
		ImGui::EndDisabled();

		if (!Tooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("%s", Tooltip.c_str());
		return bClicked && bEnabled;
	}

	// 에셋이 없어 아직 동작하지 않는 버튼. 자리와 모양만 잡아 두고 툴팁으로 이유를 보여 준다.
	void PlaceholderButton(const char* Id, UTexture2D* Icon, const char* Label, const char* Tooltip)
	{
		IconTextButton(Id, Icon, Label, FString(Tooltip) + NeedsAssetNote, false);
	}

	// 툴바 그룹 사이의 세로 구분선
	void ToolbarSeparator()
	{
		ImGui::SameLine(0.0f, ToolbarGroupSpacing);
		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		const float Height = ToolbarIconSize + ImGui::GetStyle().FramePadding.y * 2.0f;
		ImGui::GetWindowDrawList()->AddLine(Pos, ImVec2(Pos.x, Pos.y + Height), ImGui::GetColorU32(ImGuiCol_Separator));
		ImGui::Dummy(ImVec2(1.0f, Height));
		ImGui::SameLine(0.0f, ToolbarGroupSpacing);
	}
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

	LoadToolbarIcons();

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

void FParticleSystemEditorPanel::LoadToolbarIcons()
{
	const auto Load = [](const char* Name)
	{
		return UAssetManager::Get().LoadTexture(FString(EditorResourcePaths::ParticleEditorIconFolder) + Name + ".png");
	};

	Icons.Save = UAssetManager::Get().LoadTexture(EditorResourcePaths::ToolbarSaveIcon);
	Icons.RestartSim = Load("RestartSim");
	Icons.RestartLevel = Load("RestartLevel");
	Icons.Undo = Load("Undo");
	Icons.Redo = Load("Redo");
	Icons.Thumbnail = Load("Thumbnail");
	Icons.Bounds = Load("Bounds");
	Icons.OriginAxis = Load("OriginAxis");
	Icons.BackgroundColor = Load("BackgroundColor");
	Icons.RegenLOD = Load("RegenLOD");
	Icons.RegenLODPercent = Load("RegenLODPercent");
	Icons.LowestLOD = Load("LowestLOD");
	Icons.LowerLOD = Load("LowerLOD");
	Icons.AddLODBefore = Load("AddLODBefore");
	Icons.AddLODAfter = Load("AddLODAfter");
	Icons.HigherLOD = Load("HigherLOD");
	Icons.HighestLOD = Load("HighestLOD");
	Icons.DeleteLOD = Load("DeleteLOD");
	Icons.Pause = UAssetManager::Get().LoadTexture(EditorResourcePaths::ToolbarPauseIcon);
	Icons.Play = UAssetManager::Get().LoadTexture(EditorResourcePaths::ToolbarPlayIcon);
}

void FParticleSystemEditorPanel::DrawToolBar()
{
	// 버튼이 많아 창이 좁으면 넘친다. 툴바만 가로로 스크롤되게 자식 창 안에 그린다.
	const ImGuiStyle& Style = ImGui::GetStyle();
	const float ButtonHeight = std::max(ToolbarIconSize, ImGui::GetTextLineHeight()) + Style.FramePadding.y * 2.0f;
	const float ToolbarHeight = ButtonHeight + Style.ScrollbarSize + 4.0f;
	ImGui::BeginChild("##ParticleEditorToolbar", ImVec2(0.0f, ToolbarHeight), ImGuiChildFlags_None,
		ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	DrawAssetGroup();
	ToolbarSeparator();
	DrawEditGroup();
	ToolbarSeparator();
	DrawViewGroup();
	ToolbarSeparator();
	DrawLODGroup();
	ToolbarSeparator();
	DrawSimulationGroup();

	ImGui::EndChild();
	ImGui::Separator();
}

void FParticleSystemEditorPanel::DrawAssetGroup()
{
	// 저장할 파티클 시스템 에셋(UParticleSystem)이 아직 없어 비활성이다. 에셋이 생기면 Context가 저장한다.
	const bool bCanSave = Context.CanSave();
	if (IconTextButton("##Save", Icons.Save, "Save",
		bCanSave ? "Save the particle system asset" : FString("Save the particle system asset") + NeedsAssetNote, bCanSave))
		Context.Save();
	ImGui::SameLine(0.0f, ToolbarGroupSpacing);

	if (IconTextButton("##RestartSim", Icons.RestartSim, "Restart Sim", "Restart the particle system in the preview from the beginning"))
		Context.RestartSimulation();
	ImGui::SameLine();

	// 레벨에 놓인 같은 파티클 시스템을 찾으려면 컴포넌트가 어떤 에셋을 쓰는지 알아야 한다.
	PlaceholderButton("##RestartLevel", Icons.RestartLevel, "Restart Level",
		"Restart the preview and every instance of this particle system placed in the level");
}

void FParticleSystemEditorPanel::DrawEditGroup()
{
	PlaceholderButton("##Undo", Icons.Undo, "Undo", "Undo the last change to the particle system");
	ImGui::SameLine();
	PlaceholderButton("##Redo", Icons.Redo, "Redo", "Redo the last undone change");
}

void FParticleSystemEditorPanel::DrawViewGroup()
{
	PlaceholderButton("##Thumbnail", Icons.Thumbnail, "Thumbnail",
		"Save the current preview view as the asset thumbnail in the content drawer");
	ImGui::SameLine();

	PlaceholderButton("##Bounds", Icons.Bounds, "Bounds", "Show the particle system bounds in the preview");
	// 바운드 옵션: 지금 컴포넌트로 바로 동작한다. (Cascade의 바운드 옆 드롭다운과 같은 역할)
	ImGui::SameLine(0.0f, 2.0f);
	if (ImGui::ArrowButton("##BoundsOptions", ImGuiDir_Down))
		ImGui::OpenPopup("##BoundsOptionsPopup");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Bounds options");
	if (ImGui::BeginPopup("##BoundsOptionsPopup"))
	{
		const UParticleSystemComponent* Component = Context.GetPreviewComponent();
		if (ImGui::MenuItem("Set Fixed Bounds", nullptr, false, Component != nullptr))
			Context.SetFixedBoundsFromCurrent();
		if (ImGui::MenuItem("Use Dynamic Bounds", nullptr, false, Component != nullptr))
			Context.UseDynamicBounds();
		ImGui::EndPopup();
	}
	ImGui::SameLine();

	PlaceholderButton("##OriginAxis", Icons.OriginAxis, "Origin Axis", "Show the origin axis in the preview");
	ImGui::SameLine();

	// 배경색은 프리뷰 렌더 타깃의 지우기 색이라 지금 바로 동작한다.
	if (IconTextButton("##BackgroundColor", Icons.BackgroundColor, "Background Color", "Change the preview background color")
		&& ViewportPanel)
		ImGui::OpenPopup("##BackgroundColorPopup");
	if (ViewportPanel && ImGui::BeginPopup("##BackgroundColorPopup"))
	{
		ImGui::ColorPicker3("##PreviewBackground", ViewportPanel->GetBackgroundColor(),
			ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);
		ImGui::EndPopup();
	}
}

void FParticleSystemEditorPanel::DrawLODGroup()
{
	// LOD는 에셋의 이미터마다 LOD 레벨 목록이 있어야 의미가 있다. 모두 에셋 연결 후 동작한다.
	PlaceholderButton("##RegenLOD", Icons.RegenLOD, "Regen LOD", "Regenerate the lowest LOD by duplicating the highest LOD");
	ImGui::SameLine();
	PlaceholderButton("##RegenLODPercent", Icons.RegenLODPercent, "Regen LOD %",
		"Regenerate the lowest LOD from the highest LOD using percentage scaling");
	ImGui::SameLine();
	PlaceholderButton("##LowestLOD", Icons.LowestLOD, "Lowest LOD", "Jump to the lowest LOD");
	ImGui::SameLine();
	PlaceholderButton("##LowerLOD", Icons.LowerLOD, "Lower LOD", "Move to the next lower LOD");
	ImGui::SameLine();
	PlaceholderButton("##AddLODBefore", Icons.AddLODBefore, "Add LOD", "Add an LOD before the current LOD");
	ImGui::SameLine();

	// 현재 LOD 번호. 에셋이 없어 0에 고정된다.
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("LOD:");
	ImGui::SameLine();
	ImGui::BeginDisabled();
	ImGui::SetNextItemWidth(32.0f);
	ImGui::InputInt("##CurrentLOD", &Context.CurrentLODIndex, 0, 0);
	ImGui::EndDisabled();
	ImGui::SameLine();

	PlaceholderButton("##AddLODAfter", Icons.AddLODAfter, "Add LOD", "Add an LOD after the current LOD");
	ImGui::SameLine();
	PlaceholderButton("##HigherLOD", Icons.HigherLOD, "Higher LOD", "Move to the next higher LOD");
	ImGui::SameLine();
	PlaceholderButton("##HighestLOD", Icons.HighestLOD, "Highest LOD", "Jump to the highest LOD");
	ImGui::SameLine();
	PlaceholderButton("##DeleteLOD", Icons.DeleteLOD, "Delete LOD", "Delete the current LOD");
}

void FParticleSystemEditorPanel::DrawSimulationGroup()
{
	// Cascade 툴바에는 없지만 프리뷰 확인에 필요해 끝에 둔다. 같은 자리에서 일시정지/재개를 바꾼다.
	const bool bPaused = Context.bSimulationPaused;
	if (IconTextButton("##PauseSim", bPaused ? Icons.Play : Icons.Pause, bPaused ? "Resume" : "Pause",
		bPaused ? "Resume the simulation" : "Pause the simulation"))
		Context.bSimulationPaused = !bPaused;
	ImGui::SameLine();

	if (IconTextButton("##ResetCamera", nullptr, "Reset Camera", "Move the preview camera back to its start position") && ViewportPanel)
		ViewportPanel->ResetCamera();
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

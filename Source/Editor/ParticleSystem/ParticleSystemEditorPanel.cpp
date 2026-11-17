#include "EnginePCH.h"
#include "Editor/ParticleSystem/ParticleSystemEditorPanel.h"
#include "Editor/ParticleSystem/ParticleSystemViewportPanel.h"
#include "Editor/ParticleSystem/ParticleSystemEmitterPanel.h"
#include "Editor/ParticleSystem/ParticleSystemDetailsPanel.h"

#include "Editor/HitoriEd/EditorResourcePaths.h"
#include "Components/ParticleSystemComponent.h"
#include "Asset/AssetManager.h"
#include "Particles/ParticleSystem.h"
#include "Render/Texture2D.h"

#include <algorithm>

namespace
{
	constexpr const char* InnerDockSpaceName = "ParticleSystemEditorInnerDockSpace";
	constexpr const char* WindowClassName = "ParticleSystemEditorClass";

	constexpr float ToolbarIconSize = 18.0f;
	constexpr float ToolbarGroupSpacing = 10.0f;

	constexpr const char* SaveAsPopupName = "Save Particle System As##Modal";
	constexpr const char* NewAssetDefaultName = "NewParticleSystem";
	// 저장 실패 문구 색. 성공은 기본 글자색 그대로 보여 준다.
	const ImVec4 ErrorTextColor(1.0f, 0.45f, 0.35f, 1.0f);

	// 아직 동작할 수 없는 버튼의 툴팁 끝에 붙인다.
	constexpr const char* NeedsAssetNote = "\n(Disabled: needs the particle system asset, not connected yet)";
	constexpr const char* NeedsLODEditingNote = "\n(Disabled: LOD duplication and runtime LOD switching are not supported yet)";
	constexpr const char* SingleLODNote = "\n(Disabled: there is no other LOD to move to)";

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

	// 아직 동작하지 않는 버튼. 자리와 모양만 잡아 두고 툴팁으로 이유를 보여 준다.
	void PlaceholderButton(const char* Id, UTexture2D* Icon, const char* Label, const char* Tooltip,
		const char* Reason = NeedsAssetNote)
	{
		IconTextButton(Id, Icon, Label, FString(Tooltip) + Reason, false);
	}

	// 조건에 따라 켜지는 버튼. 꺼져 있을 때만 툴팁에 이유를 붙인다.
	bool ConditionalButton(const char* Id, UTexture2D* Icon, const char* Label, const char* Tooltip,
		const bool bEnabled, const char* DisabledReason)
	{
		return IconTextButton(Id, Icon, Label, bEnabled ? FString(Tooltip) : FString(Tooltip) + DisabledReason, bEnabled);
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

void FParticleSystemEditorPanel::OpenAssetFile(const FString& Path)
{
	// 경로 → 에셋 키 변환과 "이미 불러왔으면 그 객체를 돌려주기"는 LoadParticleSystem이 맡는다.
	FString Error;
	UParticleSystem* Asset = UAssetManager::Get().LoadParticleSystem(Path, Error);
	if (!Asset)
	{
		HTR_LOG(Error, "Failed to open particle system '{}': {}", Path, Error);
		return;
	}

	SetOpen(true);
	bFocusNextFrame = true;
	Context.OpenAsset(Asset);
}

void FParticleSystemEditorPanel::OnRender()
{
	bool bOpen = IsOpen();
	ImGui::SetNextWindowSize(ImVec2(1280, 800), ImGuiCond_FirstUseEver);
	// 저장하지 않은 수정이 있으면 탭 제목에 점이 붙는다. (블렌드스페이스 에디터와 같은 표시)
	const ImGuiWindowFlags HostFlags = ImGuiWindowFlags_MenuBar | (Context.IsDirty() ? ImGuiWindowFlags_UnsavedDocument : ImGuiWindowFlags_None);
	if (bFocusNextFrame)
	{
		ImGui::SetNextWindowFocus();
		bFocusNextFrame = false;
	}
	bHostVisible = ImGui::Begin(GetPanelName(), &bOpen, HostFlags);

	if (bHostVisible)
	{
		DrawMenuBar();	// ①
		DrawToolBar();	// ②
		DrawSaveAsPopup();
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

	// Ctrl+S: 하위 패널(뷰포트·이미터·디테일)이 포커스를 가진 때도 저장한다. 우클릭 카메라 이동(S키) 중에는 무시한다.
	if (bHostVisible && bOpen && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows | ImGuiFocusedFlags_DockHierarchy)
		&& ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false) && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
		RequestSave();
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

	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("Save", "Ctrl+S", false, Context.CanSave()))
			RequestSave();
		if (ImGui::MenuItem("Save As...", nullptr, false, Context.CanSave()))
			RequestSaveAs();

		// 저장한 파일과 기본 에셋 중 하나를 골라 프리뷰에서 편집한다.
		if (ImGui::BeginMenu("Open"))
		{
			const UParticleSystem* Current = Context.GetTemplate();
			for (UParticleSystem* Asset : UAssetManager::Get().GetParticleSystemAssets())
			{
				if (ImGui::MenuItem(Asset->GetPath().c_str(), nullptr, Asset == Current))
					Context.OpenAsset(Asset);
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}

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

	// 메뉴 오른쪽에 편집 중인 에셋과 저장 결과를 보여 준다. 수정이 남아 있으면 이름 뒤에 *이 붙는다.
	const FString Label = Context.GetAssetLabel();
	if (!Label.empty())
		ImGui::TextDisabled("%s%s", Label.c_str(), Context.IsDirty() ? " *" : "");
	if (!Context.GetSaveStatus().empty())
	{
		if (Context.IsSaveStatusError())
			ImGui::PushStyleColor(ImGuiCol_Text, ErrorTextColor);
		ImGui::TextUnformatted(Context.GetSaveStatus().c_str());
		if (Context.IsSaveStatusError())
			ImGui::PopStyleColor();
	}

	ImGui::EndMenuBar();
}

void FParticleSystemEditorPanel::RequestSave()
{
	if (!Context.CanSave())
		return;

	// 파일이 있으면 바로 덮어쓴다. 코드로 만든 기본 에셋은 이름을 정해야 하므로 Save As로 보낸다.
	if (Context.HasAssetFile())
		Context.Save();
	else
		RequestSaveAs();
}

void FParticleSystemEditorPanel::RequestSaveAs()
{
	// 파일이 있는 에셋을 다른 이름으로 저장할 때는 원래 이름 뒤에 _Copy를 붙여 제안한다.
	FString Suggested = NewAssetDefaultName;
	if (Context.HasAssetFile())
		Suggested = fs::path(Context.GetAssetLabel()).stem().string() + "_Copy";
	strcpy_s(SaveAsName, sizeof(SaveAsName), Suggested.c_str());

	SaveAsError.clear();
	bOpenSaveAsPopup = true;
}

void FParticleSystemEditorPanel::DrawSaveAsPopup()
{
	// 메뉴·단축키는 요청만 남기고, 팝업은 호스트 창의 ID 스택에서 여기서 연다.
	if (bOpenSaveAsPopup)
	{
		ImGui::OpenPopup(SaveAsPopupName);
		bOpenSaveAsPopup = false;
	}

	if (!ImGui::BeginPopupModal(SaveAsPopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;

	const auto TrySave = [this]()
	{
		if (Context.SaveAs(SaveAsName, SaveAsError))
			ImGui::CloseCurrentPopup();
	};

	ImGui::TextUnformatted("Enter new asset name:");
	ImGui::Spacing();
	if (ImGui::IsWindowAppearing())
		ImGui::SetKeyboardFocusHere();
	ImGui::SetNextItemWidth(350.0f);
	if (ImGui::InputText("##SaveAsName", SaveAsName, sizeof(SaveAsName), ImGuiInputTextFlags_EnterReturnsTrue))
		TrySave();
	ImGui::TextDisabled("Will be saved as: Assets/Particles/%s.particle", SaveAsName);

	if (!SaveAsError.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ErrorTextColor);
		ImGui::TextWrapped("%s", SaveAsError.c_str());
		ImGui::PopStyleColor();
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();
	if (ImGui::Button("Save", ImVec2(120.0f, 0.0f)))
		TrySave();
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
		ImGui::CloseCurrentPopup();
	ImGui::EndPopup();
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
	// 파일이 있는 에셋은 바로 저장하고, 코드로 만든 기본 에셋은 이름을 묻는 Save As 창이 뜬다.
	if (ConditionalButton("##Save", Icons.Save, "Save",
		Context.HasAssetFile() ? "Save the particle system asset (Ctrl+S)" : "Save the particle system as a new asset (Ctrl+S)",
		Context.CanSave(), NeedsAssetNote))
		RequestSave();
	ImGui::SameLine(0.0f, ToolbarGroupSpacing);

	if (IconTextButton("##RestartSim", Icons.RestartSim, "Restart Sim", "Restart the particle system in the preview from the beginning"))
		Context.RestartSimulation();
	ImGui::SameLine();

	// 같은 Template을 쓰는 프리뷰·레벨·PIE의 컴포넌트를 모두 다시 시작한다.
	if (ConditionalButton("##RestartLevel", Icons.RestartLevel, "Restart Level",
		"Restart the preview and every instance of this particle system placed in the level",
		Context.GetTemplate() != nullptr, NeedsAssetNote))
		Context.RestartLevelInstances();
}

void FParticleSystemEditorPanel::DrawViewGroup()
{
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
	// LOD 0이 가장 높은(가장 자세한) LOD다. Lower는 번호가 커지는 쪽, Higher는 작아지는 쪽이다. (Cascade와 같은 규칙)
	// LOD 레벨 생성·복제는 모듈 깊은 복사와 런타임 LOD 전환이 없어 아직 비활성이다.
	PlaceholderButton("##RegenLOD", Icons.RegenLOD, "Regen LOD",
		"Regenerate the lowest LOD by duplicating the highest LOD", NeedsLODEditingNote);
	ImGui::SameLine();
	PlaceholderButton("##RegenLODPercent", Icons.RegenLODPercent, "Regen LOD %",
		"Regenerate the lowest LOD from the highest LOD using percentage scaling", NeedsLODEditingNote);
	ImGui::SameLine();

	// 이동 버튼은 이미터 패널이 보여 줄 LOD 레벨만 바꾼다. 범위 밖이면 비활성이다.
	const int32 LODCount = Context.GetLODCount();
	Context.SetCurrentLOD(Context.CurrentLODIndex);	// 에셋의 LOD 수가 줄었을 때 범위를 다시 맞춘다.
	const int32 LowestIndex = LODCount - 1;
	const bool bCanGoLower = Context.CurrentLODIndex < LowestIndex;
	const bool bCanGoHigher = Context.CurrentLODIndex > 0;

	if (ConditionalButton("##LowestLOD", Icons.LowestLOD, "Lowest LOD", "Jump to the lowest LOD", bCanGoLower, SingleLODNote))
		Context.SetCurrentLOD(LowestIndex);
	ImGui::SameLine();
	if (ConditionalButton("##LowerLOD", Icons.LowerLOD, "Lower LOD", "Move to the next lower LOD", bCanGoLower, SingleLODNote))
		Context.SetCurrentLOD(Context.CurrentLODIndex + 1);
	ImGui::SameLine();
	PlaceholderButton("##AddLODBefore", Icons.AddLODBefore, "Add LOD", "Add an LOD before the current LOD", NeedsLODEditingNote);
	ImGui::SameLine();

	// 현재 LOD 번호. 직접 입력해도 범위 안으로 맞춘다. LOD가 하나뿐이면 바꿀 수 없다.
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("LOD:");
	ImGui::SameLine();
	ImGui::BeginDisabled(LODCount <= 1);
	ImGui::SetNextItemWidth(32.0f);
	int32 EditedLOD = Context.CurrentLODIndex;
	if (ImGui::InputInt("##CurrentLOD", &EditedLOD, 0, 0))
		Context.SetCurrentLOD(EditedLOD);
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("Current LOD (0 = highest detail, %d LOD level(s))", LODCount);
	ImGui::SameLine();

	PlaceholderButton("##AddLODAfter", Icons.AddLODAfter, "Add LOD", "Add an LOD after the current LOD", NeedsLODEditingNote);
	ImGui::SameLine();
	if (ConditionalButton("##HigherLOD", Icons.HigherLOD, "Higher LOD", "Move to the next higher LOD", bCanGoHigher, SingleLODNote))
		Context.SetCurrentLOD(Context.CurrentLODIndex - 1);
	ImGui::SameLine();
	if (ConditionalButton("##HighestLOD", Icons.HighestLOD, "Highest LOD", "Jump to the highest LOD", bCanGoHigher, SingleLODNote))
		Context.SetCurrentLOD(0);
	ImGui::SameLine();
	PlaceholderButton("##DeleteLOD", Icons.DeleteLOD, "Delete LOD", "Delete the current LOD", NeedsLODEditingNote);
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

#include "EnginePCH.h"
#include "Editor/SkeletalMeshViewer/SkeletalMeshViewerTab.h"
#include "Editor/SkeletalMeshViewer/SkeletalMeshEditorUtils.h"
#include "Editor/SkeletalMeshViewer/SkeletalViewportPanel.h"
#include "Editor/SkeletalMeshViewer/SkeletonTreePanel.h"
#include "Editor/SkeletalMeshViewer/SkeletalAssetDetailsPanel.h"
#include "Editor/SkeletalMeshViewer/SkeletalPreviewSettingsPanel.h"
#include "Editor/SkeletalMeshViewer/SkeletalAnimationPanel.h"
#include "Editor/SkeletalMeshViewer/SkeletalBlendSpacePanel.h"
#include "imgui_internal.h"
#include "UObject/UObjectIterator.h"

#include <filesystem>

using namespace SkeletalMeshEditorUtils;

namespace
{
	FEditorTabSettings MakeSettings(const FString& Id, const bool bSharedPreview)
	{
		FEditorTabSettings Settings;
		Settings.Title = bSharedPreview ? "Animation Preview" : "Skeletal Mesh Viewer";
		Settings.Id = Id;
		Settings.bClosable = true;
		// 공유 미리보기 탭은 State Machine·AnimGraph 에디터가 계속 가리키므로 닫아도 지우지 않고 숨긴다.
		Settings.bKeepWhenClosed = bSharedPreview;
		return Settings;
	}
}

FString FSkeletalMeshViewerTab::MakeTabId(const int32 InSlot)
{
	return "SkeletalMeshViewer_" + std::to_string(InSlot);
}

FSkeletalMeshViewerTab::FSkeletalMeshViewerTab(const int32 InSlot, USkeletalMesh* InMesh,
	std::function<void(USkeletalMesh*)> InOpenMeshRequest)
	: FSkeletalPreviewTabBase(MakeSettings(MakeTabId(InSlot), false))
	, Slot(InSlot)
	, InitialMesh(InMesh)
	, OpenMeshRequest(std::move(InOpenMeshRequest))
{
}

FSkeletalMeshViewerTab::FSkeletalMeshViewerTab(const FString& InId)
	: FSkeletalPreviewTabBase(MakeSettings(InId, true))
	, bSharedPreview(true)
{
}

bool FSkeletalMeshViewerTab::Init()
{
	if (!FSkeletalPreviewTabBase::Init())
		return false;

	// 메시 탭은 Mesh 메뉴에서 다른 메시를 고르면 엔진이 그 메시의 탭을 연다. 공유 미리보기 탭은 자기 안에서 바꾼다.
	if (!bSharedPreview)
		SetOpenMeshRequestHandler(OpenMeshRequest);
	if (InitialMesh)
		OpenMesh(InitialMesh);

	// 추가 순서가 기본 배치에서 같은 노드에 겹친 창의 탭 순서가 된다.
	const ImGuiWindowClass& Class = GetWindowClass();
	ViewportName        = AddPanel<FSkeletalViewportPanel>(*this, GetId(), Class)->GetPanelName();
	SkeletonTreeName    = AddPanel<FSkeletonTreePanel>(*this, GetId(), Class)->GetPanelName();
	AssetDetailsName    = AddPanel<FSkeletalAssetDetailsPanel>(*this, GetId(), Class)->GetPanelName();
	PreviewSettingsName = AddPanel<FSkeletalPreviewSettingsPanel>(*this, GetId(), Class)->GetPanelName();
	AnimationName       = AddPanel<FSkeletalAnimationPanel>(*this, GetId(), Class)->GetPanelName();
	BlendSpaceName      = AddPanel<FSkeletalBlendSpacePanel>(*this, GetId(), Class)->GetPanelName();

	RefreshTitle();
	return true;
}

void FSkeletalMeshViewerTab::Tick(const float DeltaTime)
{
	RefreshTitle();
	if (!IsVisible())
		return;

	if (bLayoutBuiltWithoutWindows && SelectedMesh)
	{
		bLayoutBuiltWithoutWindows = false;
		RequestLayoutReset();
	}

	FSkeletalPreviewTabBase::Tick(DeltaTime);
}

void FSkeletalMeshViewerTab::RefreshTitle()
{
	FString Name = bSharedPreview ? FString("Animation Preview") : FString("Skeletal Mesh Viewer");
	if (SelectedMesh && !SelectedMesh->GetPath().empty())
	{
		const FString FileName = std::filesystem::path(SelectedMesh->GetPath()).filename().string();
		Name = bSharedPreview ? "Animation Preview - " + FileName : FileName;
	}
	SetTitle(Name);
}

const UObject* FSkeletalMeshViewerTab::GetEditingAsset() const
{
	return bSharedPreview ? nullptr : SelectedMesh;
}

void FSkeletalMeshViewerTab::OnPreRender()
{
	// 가려지거나 닫힌 탭의 뷰포트가 지난 프레임 상태로 남아 계속 렌더·입력을 받지 않게 매 프레임 지운다.
	// 이번 프레임에 보이는 패널이 다시 채운다.
	BeginFrame();
}

void FSkeletalMeshViewerTab::DrawHostContents()
{
	DrawToolbarUI();
	if (bSharedPreview)
		DrawAssetTabBar();
	if (!OpenAssetMessage.empty())
		ImGui::TextWrapped("%s", OpenAssetMessage.c_str());

	// 열려 있는 메시가 없을 때의 안내. 이때 패널들은 그리지 않는다.
	if (!SelectedMesh)
	{
		ImGui::Spacing();
		ImGui::TextDisabled("No Skeletal Mesh currently open.");
		ImGui::Spacing();
		ImGui::Text("You can open a mesh from the 'Mesh' menu above, or click [Edit] next to the SkeletalMesh property in the Details Panel.");
	}
}

bool FSkeletalMeshViewerTab::NeedsLayoutRebuild(const ImGuiDockNode* Node) const
{
	if (FEditorTab::NeedsLayoutRebuild(Node))
		return true;

	// 패널 창이 지금 있거나 ini에 기록된 적이 있으면 사용자가 만든 배치로 보고 그대로 둔다.
	for (const FString* Name : { &ViewportName, &SkeletonTreeName, &AssetDetailsName, &PreviewSettingsName, &AnimationName, &BlendSpaceName })
	{
		if (ImGui::FindWindowByName(Name->c_str()) || ImGui::FindWindowSettingsByID(ImHashStr(Name->c_str())))
			return false;
	}
	return true;
}

void FSkeletalMeshViewerTab::BuildDefaultLayout(const ImGuiID DockSpaceId)
{
	bLayoutBuiltWithoutWindows = ImGui::FindWindowByName(ViewportName.c_str()) == nullptr;

	ImGuiID CenterNode = DockSpaceId;
	const ImGuiID LeftNode = ImGui::DockBuilderSplitNode(CenterNode, ImGuiDir_Left, 0.22f, nullptr, &CenterNode);
	ImGuiID RightNode = ImGui::DockBuilderSplitNode(CenterNode, ImGuiDir_Right, 0.28f, nullptr, &CenterNode);
	const ImGuiID RightBottomNode = ImGui::DockBuilderSplitNode(RightNode, ImGuiDir_Down, 0.50f, nullptr, &RightNode);
	const ImGuiID CenterBottomNode = ImGui::DockBuilderSplitNode(CenterNode, ImGuiDir_Down, 0.25f, nullptr, &CenterNode);

	ImGui::DockBuilderDockWindow(SkeletonTreeName.c_str(), LeftNode);
	ImGui::DockBuilderDockWindow(AssetDetailsName.c_str(), RightNode);
	ImGui::DockBuilderDockWindow(PreviewSettingsName.c_str(), RightBottomNode);
	ImGui::DockBuilderDockWindow(AnimationName.c_str(), CenterBottomNode);
	ImGui::DockBuilderDockWindow(BlendSpaceName.c_str(), CenterBottomNode);
	ImGui::DockBuilderDockWindow(ViewportName.c_str(), CenterNode);
}

void FSkeletalMeshViewerTab::DrawAssetTabBar()
{
	if (OpenedMeshes.Num() == 0)
		return;

	if (ImGui::BeginTabBar("##SkelMeshAssetTabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs))
	{
		for (int32 i = 0; i < OpenedMeshes.Num(); ++i)
		{
			USkeletalMesh* Mesh = OpenedMeshes[i];
			if (!Mesh) continue;

			bool bTabOpen = true;
			ImGuiTabItemFlags TabFlags = ImGuiTabItemFlags_None;
			if (MeshToSelect == Mesh)
				TabFlags |= ImGuiTabItemFlags_SetSelected;

			const FString Path = Mesh->GetPath();
			FString DisplayName = Path.empty() ? "Skeletal Mesh" : std::filesystem::path(Path).filename().string();
			if (DisplayName.empty()) DisplayName = "Mesh";

			ImGui::PushID(Mesh);
			if (ImGui::BeginTabItem(DisplayName.c_str(), &bTabOpen, TabFlags))
			{
				if (SelectedMesh != Mesh)
					SetSelectedMesh(Mesh);
				ImGui::EndTabItem();
			}
			ImGui::PopID();

			if (!bTabOpen)
			{
				OpenedMeshes.RemoveAt(i);
				--i;
				if (SelectedMesh == Mesh)
					SetSelectedMesh(OpenedMeshes.IsEmpty() ? nullptr : OpenedMeshes[0]);
			}
		}

		MeshToSelect = nullptr;
		ImGui::EndTabBar();
	}
}

void FSkeletalMeshViewerTab::DrawToolbarUI()
{
	if (ImGui::BeginMenuBar())
	{
		// 1. 메시 추가 메뉴
		if (ImGui::BeginMenu("Mesh"))
		{
			if (SelectedMesh)
			{
				const bool bEditLocked = Animation.HasActivePreview();
				if (ImGui::MenuItem("Save Asset (.bin)", "Ctrl+S", false, !bEditLocked))
				{
					AssetSaver.SaveCurrentMesh();
				}
				ShowEditTooltip(bEditLocked);
				if (ImGui::MenuItem("Save As New .bin...", nullptr, false, !bEditLocked))
				{
					bShowSaveAsModal = true;
				}
				ShowEditTooltip(bEditLocked);
				if (ImGui::MenuItem("Restore to Original FBX Pose", nullptr, false, !bEditLocked))
				{
					AssetSaver.RestoreToOriginalFBXPose();
				}
				ShowEditTooltip(bEditLocked);
				ImGui::Separator();
			}

			for (TObjectIterator<USkeletalMesh> Itr; Itr; ++Itr)
			{
				USkeletalMesh* Mesh = *Itr;
				if (!Mesh) continue;

				FString Path = Mesh->GetPath();
				FString Name = Path.empty() ? "Mesh" : std::filesystem::path(Path).filename().string();
				const bool bCurrent = (Mesh == SelectedMesh);

				if (ImGui::MenuItem(Name.c_str(), nullptr, bCurrent))
				{
					// 탭 방식이면 엔진이 그 메시의 탭을 연다. 메시가 없는 빈 뷰어는 자기 자리에서 연다.
					if (OnOpenMeshRequested && SelectedMesh && !bCurrent)
						OnOpenMeshRequested(Mesh);
					else
						OpenMesh(Mesh);
				}
			}
			ImGui::EndMenu();
		}

		// 2. 뷰 모드 메뉴
		if (ImGui::BeginMenu("View Mode"))
		{
			// 표시 모드: 라디오 그룹. 체크는 DisplayMode 하나로 계산하므로 항상 하나만 켜진다.
			struct FDisplayModeItem
			{
				const char* Label;
				FSkeletalPreviewTabBase::EViewerDisplayMode Mode;
			};
			static constexpr FDisplayModeItem DisplayModeItems[] =
			{
				{ "Lit (Blinn-Phong)", FSkeletalPreviewTabBase::EViewerDisplayMode::LitBlinnPhong },
				{ "Lit (Lambert)",     FSkeletalPreviewTabBase::EViewerDisplayMode::LitLambert },
				{ "Unlit",             FSkeletalPreviewTabBase::EViewerDisplayMode::Unlit },
				{ "World Normal",      FSkeletalPreviewTabBase::EViewerDisplayMode::WorldNormal },
				{ "Bone Weight",       FSkeletalPreviewTabBase::EViewerDisplayMode::BoneWeightHeatmap },
			};
			for (const FDisplayModeItem& Item : DisplayModeItems)
			{
				if (ImGui::MenuItem(Item.Label, nullptr, DisplayMode == Item.Mode))
				{
					DisplayMode = Item.Mode;
				}
			}

			// 독립 토글: 표시 모드와 상관없이 켜고 끈다.
			ImGui::Separator();
			ImGui::MenuItem("Wireframe", nullptr, &bWireframe);

			ImGui::EndMenu();
		}

		// 3. 윈도우 메뉴
		if (ImGui::BeginMenu("Window"))
		{
			ImGui::MenuItem("Skeletal Mesh Viewport", nullptr, &bViewportOpen);
			ImGui::MenuItem("Skeleton Tree", nullptr, &bSkeletonTreeOpen);
			ImGui::MenuItem("Asset Details", nullptr, &bAssetDetailsOpen);
			ImGui::MenuItem("Preview Scene Settings", nullptr, &bPreviewSettingsOpen);
			ImGui::MenuItem("Animation", nullptr, &bAnimationOpen);
			ImGui::MenuItem("Blend Space", nullptr, &bBlendSpaceOpen);
			ImGui::Separator();
			if (ImGui::MenuItem("Reset Layout"))
			{
				// 배치 초기화는 탭이 한다. (FEditorTab이 다음 프레임에 BuildDefaultLayout을 다시 부른다)
				RequestLayoutReset();
				bViewportOpen = true;
				bSkeletonTreeOpen = true;
				bAssetDetailsOpen = true;
				bPreviewSettingsOpen = true;
				bAnimationOpen = true;
				bBlendSpaceOpen = true;
			}
			ImGui::EndMenu();
		}

		// 4. 저장 및 복원 관련 툴바 버튼
		if (SelectedMesh)
		{
			ImGui::Separator();
			const bool bEditLocked = Animation.HasActivePreview();

			ImGui::BeginDisabled(bEditLocked);
			if (ImGui::Button("Save Asset"))
			{
				AssetSaver.SaveCurrentMesh();
			}
			ImGui::EndDisabled();
			ShowEditTooltip(bEditLocked, "Save modified bone transforms to current .bin file (Ctrl+S)");

			ImGui::SameLine();
			ImGui::BeginDisabled(bEditLocked);
			if (ImGui::Button("Save As New .bin"))
			{
				bShowSaveAsModal = true;
			}
			ImGui::EndDisabled();
			ShowEditTooltip(bEditLocked, "Save current modified pose as a new .fbx.bin asset and register to editor");

			ImGui::SameLine();
			ImGui::BeginDisabled(bEditLocked);
			if (ImGui::Button("Restore to Original FBX Pose"))
			{
				AssetSaver.RestoreToOriginalFBXPose();
			}
			ImGui::EndDisabled();
			ShowEditTooltip(bEditLocked, "Discard modified pose and restore .bin to original FBX bind pose");

			if (SaveStatusTimer > 0.0f)
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "%s", SaveStatusMessage.c_str());
			}
		}

		ImGui::EndMenuBar();
	}

	DrawSaveAsModal();
}

void FSkeletalMeshViewerTab::DrawSaveAsModal()
{
	if (bShowSaveAsModal)
	{
		ImGui::OpenPopup("Save As New .bin##Modal");
		bShowSaveAsModal = false;

		if (SelectedMesh)
		{
			std::filesystem::path Path(SelectedMesh->GetPath());
			FString Stem = Path.stem().string();
			if (Stem.ends_with(".fbx")) Stem = Stem.substr(0, Stem.length() - 4);
			Stem += "_Modified";
			strcpy_s(SaveAsNameBuffer, sizeof(SaveAsNameBuffer), Stem.c_str());
		}
		else
		{
			strcpy_s(SaveAsNameBuffer, sizeof(SaveAsNameBuffer), "NewSkeletalMesh");
		}
	}

	if (ImGui::BeginPopupModal("Save As New .bin##Modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Enter new asset name:");
		ImGui::Spacing();
		ImGui::SetNextItemWidth(350.0f);
		if (ImGui::InputText("##NewBinNameInput", SaveAsNameBuffer, sizeof(SaveAsNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			if (strlen(SaveAsNameBuffer) > 0)
			{
				AssetSaver.SaveAsNewBin(SaveAsNameBuffer);
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::TextDisabled("Will be saved as: %s.fbx.bin", SaveAsNameBuffer);
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Save", ImVec2(120.0f, 0.0f)))
		{
			if (strlen(SaveAsNameBuffer) > 0)
			{
				AssetSaver.SaveAsNewBin(SaveAsNameBuffer);
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}


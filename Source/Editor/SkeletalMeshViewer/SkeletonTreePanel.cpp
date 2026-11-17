#include "EnginePCH.h"
#include "Editor/SkeletalMeshViewer/SkeletonTreePanel.h"
#include "Editor/SkeletalMeshViewer/SkeletalPreviewTabBase.h"
#include "Editor/SkeletalMeshViewer/SkeletalMeshEditorUtils.h"

#include "Animation/AssetAnimInstance.h"
#include "Animation/AnimGraph.h"
#include "Asset/AssetManager.h"
#include "Camera/MinimalViewInfo.h"
#include "Collision/Ray.h"
#include "Engine/SceneManagement.h"
#include "Input/InputSystem.h"
#include "Render/RenderCommand.h"
#include "Math/EngineMath.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "Render/Material.h"
#include "Render/RenderResourceManager.h"
#include "Components/SkinnedMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Core/Misc/FileHelper.h"
#include "Animation/AnimNotifyFile.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotify_CameraShake.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Engine/Engine.h"
#include "Audio/AudioSystem.h"
#include "Camera/Shake/CameraShakePattern.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectGlobals.h"
#include "Core/Misc/FileHelper.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>

using namespace SkeletalMeshEditorUtils;

void FSkeletonTreePanel::OnRender()
{
	// 메시가 없거나 이 창을 닫았으면 그리지 않는다. 창의 열림 상태는 탭이 가진다. (탭의 Window 메뉴가 같은 값을 바꾼다)
	if (!Tab.SelectedMesh || !Tab.bSkeletonTreeOpen)
	{
		return;
	}
	ImGui::SetNextWindowClass(&WindowClass);
	if (ImGui::Begin(PanelName.c_str(), &Tab.bSkeletonTreeOpen))
	{
		DrawBoneHierarchyUI();
	}
	ImGui::End();
}

void FSkeletonTreePanel::DrawBoneHierarchyUI()
{
	ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "--- Skeleton Tree (Bone Hierarchy) ---");

	if (!Tab.SelectedMesh)
	{
		ImGui::TextDisabled("No Skeletal Mesh selected.");
		return;
	}

	const TArray<FSkeletalBone>& Bones = Tab.SelectedMesh->MeshData.Bones;
	if (Bones.Num() == 0)
	{
		ImGui::TextDisabled("No bones in this skeletal mesh.");
		return;
	}

	const bool bEditLocked = Tab.Animation.HasActivePreview();
	ImGui::BeginDisabled(bEditLocked);
	if (ImGui::Button("Reset All Bones to Bind Pose", ImVec2(-1.0f, 0.0f)))
	{
		if (Tab.PreviewComponent)
		{
			Tab.PreviewComponent->ResetPoseToBindPose();
		}
	}
	ImGui::EndDisabled();
	ShowEditTooltip(bEditLocked);

	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint("##BoneFilter", "Search bone name...", BoneSearchFilter, sizeof(BoneSearchFilter));

	const float FooterHeight = (Tab.BoneEdit.SelectedBoneIndex >= 0 && Tab.BoneEdit.SelectedBoneIndex < Bones.Num())
		? ImGui::GetFrameHeightWithSpacing() * 9.2f
		: ImGui::GetFrameHeightWithSpacing() * 1.5f;

	ImGui::BeginChild("BoneHierarchyScroll", ImVec2(0, -FooterHeight), true);
	{
		const bool bHasFilter = (strlen(BoneSearchFilter) > 0);

		if (bHasFilter)
		{
			for (int32 i = 0; i < Bones.Num(); ++i)
			{
				if (Bones[i].Name.find(BoneSearchFilter) != std::string::npos)
				{
					const bool bSelected = (Tab.BoneEdit.SelectedBoneIndex == i);
					char Label[128];
					sprintf_s(Label, sizeof(Label), "[%d] %s (Parent: %d)", i, Bones[i].Name.c_str(), Bones[i].ParentIndex);
					if (ImGui::Selectable(Label, bSelected))
					{
						Tab.BoneEdit.SelectedBoneIndex = i;
					}
				}
			}
		}
		else
		{
			for (int32 i = 0; i < Bones.Num(); ++i)
			{
				if (Bones[i].ParentIndex == -1)
				{
					DrawBoneNode(i, Bones);
				}
			}
		}

		// 빈 공간에서도 스크롤 플래그 클리어 보장
		Tab.BoneEdit.bScrollToSelectedBone = false;
	}
	ImGui::EndChild();

	if (Tab.BoneEdit.SelectedBoneIndex >= 0 && Tab.BoneEdit.SelectedBoneIndex < Bones.Num())
	{
		const FSkeletalBone& SelBone = Bones[Tab.BoneEdit.SelectedBoneIndex];
		ImGui::Separator();
		ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.25f, 1.0f), "Selected: [%d] %s", Tab.BoneEdit.SelectedBoneIndex, SelBone.Name.c_str());

		if (SelBone.ParentIndex >= 0 && SelBone.ParentIndex < Bones.Num())
		{
			ImGui::Text("Parent: [%d] %s", SelBone.ParentIndex, Bones[SelBone.ParentIndex].Name.c_str());
		}
		else
		{
			ImGui::Text("Parent: None (Root Bone)");
		}

		const bool bEditLocked = Tab.Animation.HasActivePreview();
		ImGui::BeginDisabled(bEditLocked);
		if (Tab.PreviewComponent)
		{
			FTransform LocalTf = Tab.BoneEdit.GetBoneLocalTransform(Tab.BoneEdit.SelectedBoneIndex);
			bool bChanged = false;
			bChanged |= ImGui::DragFloat3("Local Loc", &LocalTf.Location.X, 0.05f, -10000.0f, 10000.0f, "%.2f");

			float RotArr[3] = { LocalTf.Rotation.Pitch, LocalTf.Rotation.Yaw, LocalTf.Rotation.Roll };
			if (ImGui::DragFloat3("Local Rot (P/Y/R)", RotArr, 0.5f, -360.0f, 360.0f, "%.1f"))
			{
				LocalTf.Rotation.Pitch = RotArr[0];
				LocalTf.Rotation.Yaw = RotArr[1];
				LocalTf.Rotation.Roll = RotArr[2];
				bChanged = true;
			}

			bChanged |= ImGui::DragFloat3("Local Scale", &LocalTf.Scale.X, 0.01f, 0.01f, 100.0f, "%.2f");

			if (bChanged)
			{
				Tab.PreviewComponent->SetBoneLocalTransform(Tab.BoneEdit.SelectedBoneIndex, LocalTf);
			}
		}

		if (ImGui::Button("Reset Bone", ImVec2(-1.0f, 0.0f)))
		{
			if (Tab.PreviewComponent)
			{
				Tab.PreviewComponent->SetBoneLocalTransform(Tab.BoneEdit.SelectedBoneIndex, SelBone.RefPoseTransform);
			}
		}
		ImGui::EndDisabled();
		ShowEditTooltip(bEditLocked);
		if (ImGui::Button("Focus Bone", ImVec2(-1.0f, 0.0f)))
		{
			TArray<FMatrix> BoneMatrices;
			Tab.BoneEdit.ComputeBoneWorldMatrices(BoneMatrices);
			if (Tab.BoneEdit.SelectedBoneIndex < BoneMatrices.Num())
			{
				const FMatrix& Mat = BoneMatrices[Tab.BoneEdit.SelectedBoneIndex];
				Tab.Camera.TargetPosition = FVector(Mat.M[3][0], Mat.M[3][1], Mat.M[3][2]);
				const FRotator Rot(Tab.Camera.CameraPitch, Tab.Camera.CameraYaw, 0.0f);
				Tab.Camera.CameraLocation = Tab.Camera.TargetPosition - Rot.Quaternion().GetForwardVector() * Tab.Camera.CameraDistance;
			}
		}
		if (ImGui::Button("Deselect", ImVec2(-1.0f, 0.0f)))
		{
			Tab.BoneEdit.SelectedBoneIndex = -1;
			Tab.BoneEdit.BoneGizmo.SetTarget(nullptr, false);
		}
	}
	else
	{
		ImGui::TextDisabled("Click a bone in the tree or viewport to inspect & manipulate.");
	}
}

void FSkeletonTreePanel::DrawBoneNode(int32 BoneIndex, const TArray<FSkeletalBone>& Bones)
{
	if (BoneIndex < 0 || BoneIndex >= Bones.Num())
		return;

	const FSkeletalBone& Bone = Bones[BoneIndex];

	TArray<int32> ChildIndices;
	for (int32 i = 0; i < Bones.Num(); ++i)
	{
		if (Bones[i].ParentIndex == BoneIndex)
		{
			ChildIndices.Add(i);
		}
	}

	// 뷰포트에서 본을 피킹했을 때 부모 노드를 자동으로 펼쳐주고 기본으로 상위 계층을 열어둠
	if (Tab.BoneEdit.bScrollToSelectedBone && Tab.BoneEdit.IsBoneAncestorOf(BoneIndex, Tab.BoneEdit.SelectedBoneIndex, Bones))
	{
		ImGui::SetNextItemOpen(true, ImGuiCond_Always);
	}

	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
	if (Tab.BoneEdit.SelectedBoneIndex == BoneIndex)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Selected;
	}
	if (ChildIndices.Num() == 0)
	{
		NodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	char Label[128];
	sprintf_s(Label, sizeof(Label), "[%d] %s", BoneIndex, Bone.Name.c_str());

	const bool bIsSelected = (Tab.BoneEdit.SelectedBoneIndex == BoneIndex);
	if (bIsSelected)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.25f, 1.0f));
	}

	const bool bNodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)BoneIndex, NodeFlags, "%s", Label);

	if (bIsSelected)
	{
		ImGui::PopStyleColor();
		if (Tab.BoneEdit.bScrollToSelectedBone)
		{
			ImGui::SetScrollHereY(0.5f);
			Tab.BoneEdit.bScrollToSelectedBone = false;
		}
	}

	if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
	{
		Tab.BoneEdit.SelectedBoneIndex = BoneIndex;
	}

	if (ChildIndices.Num() > 0 && bNodeOpen)
	{
		for (int32 ChildIdx : ChildIndices)
		{
			DrawBoneNode(ChildIdx, Bones);
		}
		ImGui::TreePop();
	}
}


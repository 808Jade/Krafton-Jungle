#include "EnginePCH.h"
#include "Editor/SkeletalMeshViewer/SkeletalAssetDetailsPanel.h"
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

void FSkeletalAssetDetailsPanel::OnRender()
{
	// 메시가 없거나 이 창을 닫았으면 그리지 않는다. 창의 열림 상태는 탭이 가진다. (탭의 Window 메뉴가 같은 값을 바꾼다)
	if (!Tab.SelectedMesh || !Tab.bAssetDetailsOpen)
	{
		return;
	}
	ImGui::SetNextWindowClass(&WindowClass);
	if (ImGui::Begin(PanelName.c_str(), &Tab.bAssetDetailsOpen))
	{
		DrawMeshDetailsUI();
	}
	ImGui::End();
}

void FSkeletalAssetDetailsPanel::DrawMeshDetailsUI()
{
	ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "--- Skeletal Mesh Statistics ---");

	if (!Tab.SelectedMesh)
	{
		ImGui::TextDisabled("No Skeletal Mesh selected.");
		return;
	}

	const FSkeletalMeshData& Data = Tab.SelectedMesh->GetMeshData();

	FString Path = Tab.SelectedMesh->GetPath();
	FString DisplayName = Path.empty() ? "Skeletal Mesh" : std::filesystem::path(Path).filename().string();
	ImGui::Text("Asset: %s", DisplayName.c_str());
	ImGui::Text("Path: %s", Path.c_str());
	ImGui::Text("Bones: %d", Data.Bones.Num());
	int32 VertexCount = 0;
	int32 IndexCount = 0;
	for (const FSkeletalMesh& Part : Data.Meshes)
	{
		VertexCount += Part.Vertices.Num();
		IndexCount += Part.Indices.Num();
	}
	ImGui::Text("Vertices: %d", VertexCount);
	ImGui::Text("Triangles: %d", IndexCount / 3);
	ImGui::Text("Sections: %d", Tab.SelectedMesh->Sections.Num());
	ImGui::Text("Material Slots: %d", Tab.SelectedMesh->Materials.Num());

	const FFbxAssetSet* Assets = UAssetManager::Get().FindFbxAssets(Path);
	if (Assets && !Assets->StaticMeshes.IsEmpty() &&
		ImGui::CollapsingHeader("Separate Static Meshes", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (const FFbxStaticAsset& Part : Assets->StaticMeshes)
		{
			if (!Part.Mesh) continue;
			const FString& AssetPath = Part.Mesh->GetPath();
			const FString AssetName = AssetPath.substr(AssetPath.find_last_of('/') + 1);
			const int32 VisibleIndex = Tab.VisibleStaticMeshes.Find(Part.Mesh);
			bool bVisible = VisibleIndex != INDEX_NONE;
			ImGui::PushID(Part.Mesh);
			if (ImGui::Checkbox("##ShowStaticMesh", &bVisible))
			{
				if (bVisible)
					Tab.VisibleStaticMeshes.Add(Part.Mesh);
				else
					Tab.VisibleStaticMeshes.RemoveAt(VisibleIndex);
			}
			ImGui::SameLine();
			ImGui::TextUnformatted(AssetName.c_str());
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", AssetPath.c_str());
			ImGui::PopID();
		}
	}

	const FBox& Bounds = Tab.PreviewBounds;
	const FVector Size = Bounds.Max - Bounds.Min;
	ImGui::Text("Bounds Size: (%.2f, %.2f, %.2f)", Size.X, Size.Y, Size.Z);

	if (ImGui::CollapsingHeader("Material Slots", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const char* Samplers[] = { "Clamp", "Wrap" };
		int32 SlotIndex = 0;
		for (const FSkeletalMesh& Part : Data.Meshes)
		{
			for (const FSkeletalMaterialSlot& Slot : Part.MaterialSlots)
			{
				ImGui::PushID(SlotIndex);
				ImGui::BulletText("[%d] %s", SlotIndex, Slot.Name.c_str());
				if (!Slot.BaseColorTexturePath.empty())
					ImGui::TextWrapped("Diffuse: %s", Slot.BaseColorTexturePath.c_str());

				if (const UMaterial* Material = Tab.GetPreviewMaterial(SlotIndex))
				{
					int Mode = Material->SamplerState == ESamplerState::LinearWrap ? 1 : 0;
					if (ImGui::Combo("Sampler", &Mode, Samplers, IM_ARRAYSIZE(Samplers)))
						Tab.SetPreviewSampler(SlotIndex, Mode == 1 ? ESamplerState::LinearWrap : ESamplerState::LinearClamp);
				}
				ImGui::PopID();
				++SlotIndex;
			}
		}
	}
}


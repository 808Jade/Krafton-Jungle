#include "EnginePCH.h"
#include "Editor/SkeletalMeshViewer/SkeletalPreviewSettingsPanel.h"
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

void FSkeletalPreviewSettingsPanel::OnRender()
{
	// 메시가 없거나 이 창을 닫았으면 그리지 않는다. 창의 열림 상태는 탭이 가진다. (탭의 Window 메뉴가 같은 값을 바꾼다)
	if (!Tab.SelectedMesh || !Tab.bPreviewSettingsOpen)
	{
		return;
	}
	ImGui::SetNextWindowClass(&WindowClass);
	if (ImGui::Begin(PanelName.c_str(), &Tab.bPreviewSettingsOpen))
	{
		DrawLightingSettingsUI();
	}
	ImGui::End();
}

void FSkeletalPreviewSettingsPanel::DrawLightingSettingsUI()
{
	ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "--- Preview Scene Settings ---");
	ImGui::TextWrapped("Hold 'L' and drag LMB in Viewport to rotate Directional Light in real-time.");
	ImGui::Spacing();

	if (ImGui::CollapsingHeader("Camera & Gizmo Settings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::SliderFloat("Camera Speed", &Tab.Camera.CameraSpeed, 1.0f, 5000.0f, "%.1f");
		ImGui::SliderFloat("Camera FOV", &Tab.Camera.FovDegrees, 20.0f, 120.0f, "%.1f deg");

		ImGui::Checkbox("Show Gizmo", &Tab.BoneEdit.bShowBoneGizmo);
		ImGui::Text("Gizmo Mode (W / E / R):");
		const EGizmoMode CurMode = Tab.BoneEdit.BoneGizmo.GetMode();
		if (ImGui::RadioButton("Rotate (E)", CurMode == EGizmoMode::Rotation))
		{
			Tab.BoneEdit.BoneGizmo.SetMode(EGizmoMode::Rotation);
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Translate (W)", CurMode == EGizmoMode::Location))
		{
			Tab.BoneEdit.BoneGizmo.SetMode(EGizmoMode::Location);
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Scale (R)", CurMode == EGizmoMode::Scale))
		{
			Tab.BoneEdit.BoneGizmo.SetMode(EGizmoMode::Scale);
		}

		const EGizmoSpace CurSpace = Tab.BoneEdit.BoneGizmo.GetSpace();
		ImGui::Text("Gizmo Space:");
		ImGui::SameLine();
		if (ImGui::RadioButton("Local", CurSpace == EGizmoSpace::Local))
		{
			Tab.BoneEdit.BoneGizmo.SetSpace(EGizmoSpace::Local);
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("World", CurSpace == EGizmoSpace::World))
		{
			Tab.BoneEdit.BoneGizmo.SetSpace(EGizmoSpace::World);
		}
	}

	if (ImGui::CollapsingHeader("Skeleton Display", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Show Bones", &Tab.BoneEdit.bShowBones);

		ImGui::Text("Bone Style:");
		ImGui::SameLine();
		if (ImGui::RadioButton("3D Cone", Tab.BoneEdit.bDrawBoneCones))
		{
			Tab.BoneEdit.bDrawBoneCones = true;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Line", !Tab.BoneEdit.bDrawBoneCones))
		{
			Tab.BoneEdit.bDrawBoneCones = false;
		}

		ImGui::Checkbox("Show All Bone Names", &Tab.BoneEdit.bShowAllBoneNames);
		if (Tab.BoneEdit.bDrawBoneCones)
		{
			ImGui::SliderFloat("Cone Width Ratio", &Tab.BoneEdit.BoneConeWidthRatio, 0.03f, 0.30f, "%.2f");
		}
		ImGui::SliderFloat("Joint Sphere Size", &Tab.BoneEdit.BoneJointRadius, 0.0f, 10.0f, "%.1f");
	}

	if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Enable Directional Light", &Tab.bEnableDirectionalLight);
		if (Tab.bEnableDirectionalLight)
		{
			ImGui::SliderFloat("Light Yaw", &Tab.LightYaw, 0.0f, 360.0f, "%.1f deg");
			ImGui::SliderFloat("Light Pitch", &Tab.LightPitch, -89.0f, 89.0f, "%.1f deg");
			ImGui::ColorEdit3("Light Color", &Tab.LightColor.X);
			ImGui::SliderFloat("Light Intensity", &Tab.LightIntensity, 0.0f, 10.0f, "%.2f");
		}
	}

	if (ImGui::CollapsingHeader("SkyLight & Ambient", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Enable SkyLight", &Tab.bEnableSkyLight);
		if (Tab.bEnableSkyLight)
		{
			ImGui::ColorEdit3("Sky Color", &Tab.SkyColor.X);
			ImGui::ColorEdit3("Ground Color", &Tab.GroundColor.X);
			ImGui::SliderFloat("Ambient Intensity", &Tab.SkyLightIntensity, 0.0f, 5.0f, "%.2f");
		}
	}

	if (ImGui::CollapsingHeader("Viewport Environment", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::ColorEdit4("Clear Color", &Tab.BackgroundClearColor.X);
		ImGui::Checkbox("Show Floor Grid", &Tab.bShowGrid);
		ImGui::Checkbox("Show Wireframe", &Tab.bWireframe);
	}

	ImGui::Spacing();
	if (ImGui::Button("Reset Lighting to Defaults", ImVec2(-1.0f, 26.0f)))
	{
		Tab.ResetLighting();
	}
}


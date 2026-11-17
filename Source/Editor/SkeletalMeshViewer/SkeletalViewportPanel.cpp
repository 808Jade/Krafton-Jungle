#include "EnginePCH.h"
#include "Editor/SkeletalMeshViewer/SkeletalViewportPanel.h"
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

void FSkeletalViewportPanel::OnRender()
{
	// 메시가 없거나 이 창을 닫았으면 그리지 않는다. 창의 열림 상태는 탭이 가진다. (탭의 Window 메뉴가 같은 값을 바꾼다)
	if (!Tab.SelectedMesh || !Tab.bViewportOpen)
	{
		return;
	}
	ImGuiWindowClass ViewportClass = WindowClass;
	ViewportClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoUndocking;
	ImGui::SetNextWindowClass(&ViewportClass);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	const bool bVisible = ImGui::Begin(PanelName.c_str(), &Tab.bViewportOpen,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::PopStyleVar();

	if (bVisible)
	{
		const ImVec2 AvailSize = ImGui::GetContentRegionAvail();
		const uint32 TargetWidth = static_cast<uint32>(std::max(1.0f, AvailSize.x));
		const uint32 TargetHeight = static_cast<uint32>(std::max(1.0f, AvailSize.y));

		Tab.ResizeViewport(TargetWidth, TargetHeight);
		if (Tab.bFrameRequested)
		{
			Tab.Camera.FocusOnMesh();
			Tab.bFrameRequested = false;
		}

		FTexture2D* ColorTarget = Tab.PreviewViewport.GetColorTarget();
		if (ColorTarget && ColorTarget->GetSRV())
		{
			const ImVec2 CursorPos = ImGui::GetCursorScreenPos();
			const ImVec2 ImageSize(static_cast<float>(Tab.ViewportWidth), static_cast<float>(Tab.ViewportHeight));
			Tab.bViewportVisible = true;
			Tab.bViewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
			Tab.CachedViewportOrigin = CursorPos;
			Tab.CachedViewportSize = ImageSize;
			ImGui::Image(ColorTarget->GetSRV(), ImageSize);
			Tab.bViewportHovered = ImGui::IsItemHovered();

			// 2단계: 뷰포트 본 이름 텍스트 라벨 및 선택 하이라이트 태그 오버레이
			DrawSkeletonOverlay(CursorPos, ImageSize);

			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			const char* HintText = "LMB: Pick Bone | RMB+WASD: Fly | Alt+LMB: Orbit | Wheel: Zoom | MMB: Pan | F: Focus | L+LMB: Light";
			const ImVec2 TextSize = ImGui::CalcTextSize(HintText);
			DrawList->AddRectFilled(
				ImVec2(CursorPos.x + 8, CursorPos.y + 8),
				ImVec2(CursorPos.x + TextSize.x + 16, CursorPos.y + TextSize.y + 14),
				IM_COL32(0, 0, 0, 160), 4.0f);
			DrawList->AddText(ImVec2(CursorPos.x + 12, CursorPos.y + 10), IM_COL32(230, 230, 230, 240), HintText);

			if (Tab.Animation.HasActivePreview())
			{
				const char* LockText = "ANIMATION PREVIEW  -  editing locked (select None in the active playback tab to edit)";
				const ImVec2 LockSize = ImGui::CalcTextSize(LockText);
				const float LockY = CursorPos.y + TextSize.y + 20;
				DrawList->AddRectFilled(
					ImVec2(CursorPos.x + 8, LockY - 2),
					ImVec2(CursorPos.x + LockSize.x + 16, LockY + LockSize.y + 4),
					IM_COL32(150, 90, 0, 200), 4.0f);
				DrawList->AddText(ImVec2(CursorPos.x + 12, LockY), IM_COL32(255, 235, 200, 255), LockText);
			}
		}
	}
	else
	{
		Tab.bViewportHovered = false;
		Tab.BoneEdit.HoveredBoneIndex = -1;
	}
	ImGui::End();
}

void FSkeletalViewportPanel::DrawSkeletonOverlay(const ImVec2& ViewportOrigin, const ImVec2& ViewportSize)
{
	if (!Tab.SelectedMesh || ViewportSize.x <= 1.0f || ViewportSize.y <= 1.0f)
		return;

	const TArray<FSkeletalBone>& Bones = Tab.SelectedMesh->MeshData.Bones;
	const int32 NumBones = Bones.Num();
	if (NumBones == 0)
		return;

	if (!Tab.BoneEdit.bShowBones && Tab.BoneEdit.SelectedBoneIndex < 0)
		return;

	TArray<FMatrix> BoneMatrices;
	Tab.BoneEdit.ComputeBoneWorldMatrices(BoneMatrices);
	if (BoneMatrices.Num() != NumBones)
		return;

	const FMatrix ViewProj = Tab.Camera.CachedViewMatrix * Tab.Camera.CachedProjMatrix;

	auto ProjectPoint = [&](const FVector& WorldPos, ImVec2& OutScreen) -> bool
	{
		const FVector4 Clip = ViewProj.TransformFVector4(FVector4(WorldPos.X, WorldPos.Y, WorldPos.Z, 1.0f));
		if (Clip.W <= 0.1f)
			return false;

		const float InvW = 1.0f / Clip.W;
		const float NdcX = Clip.X * InvW;
		const float NdcY = Clip.Y * InvW;

		OutScreen.x = ViewportOrigin.x + (NdcX * 0.5f + 0.5f) * ViewportSize.x;
		OutScreen.y = ViewportOrigin.y + (1.0f - (NdcY * 0.5f + 0.5f)) * ViewportSize.y;
		return true;
	};

	TArray<ImVec2> BoneScreenPts;
	TArray<uint8> BoneInFront;
	BoneScreenPts.SetNum(NumBones);
	BoneInFront.SetNum(NumBones);

	for (int32 i = 0; i < NumBones; ++i)
	{
		const FMatrix& Mat = BoneMatrices[i];
		const FVector Pos(Mat.M[3][0], Mat.M[3][1], Mat.M[3][2]);
		BoneInFront[i] = ProjectPoint(Pos, BoneScreenPts[i]) ? 1 : 0;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->PushClipRect(
		ViewportOrigin,
		ImVec2(ViewportOrigin.x + ViewportSize.x, ViewportOrigin.y + ViewportSize.y),
		true);

	// 1. 모든 본 이름 라벨 표시 (옵션 활성화 시)
	if (Tab.BoneEdit.bShowAllBoneNames)
	{
		for (int32 i = 0; i < NumBones; ++i)
		{
			if (!BoneInFront[i] || i == Tab.BoneEdit.SelectedBoneIndex)
				continue;

			const ImVec2& Pt = BoneScreenPts[i];
			DrawList->AddText(ImVec2(Pt.x + 6.0f, Pt.y - 6.0f), IM_COL32(215, 230, 245, 210), Bones[i].Name.c_str());
		}
	}

	// 2. 선택된 본의 이름 태그 오버레이 표시
	if (Tab.BoneEdit.SelectedBoneIndex >= 0 && Tab.BoneEdit.SelectedBoneIndex < NumBones && BoneInFront[Tab.BoneEdit.SelectedBoneIndex])
	{
		const ImVec2& SelScreen = BoneScreenPts[Tab.BoneEdit.SelectedBoneIndex];

		char BoneTag[160];
		sprintf_s(BoneTag, sizeof(BoneTag), "[%d] %s", Tab.BoneEdit.SelectedBoneIndex, Bones[Tab.BoneEdit.SelectedBoneIndex].Name.c_str());
		const ImVec2 TagSize = ImGui::CalcTextSize(BoneTag);
		const ImVec2 TagMin(SelScreen.x + 10.0f, SelScreen.y - TagSize.y - 8.0f);
		const ImVec2 TagMax(TagMin.x + TagSize.x + 12.0f, TagMin.y + TagSize.y + 6.0f);

		DrawList->AddRectFilled(TagMin, TagMax, IM_COL32(20, 20, 20, 215), 4.0f);
		DrawList->AddRect(TagMin, TagMax, IM_COL32(255, 185, 30, 255), 4.0f, 0, 1.5f);
		DrawList->AddText(ImVec2(TagMin.x + 6.0f, TagMin.y + 3.0f), IM_COL32(255, 230, 110, 255), BoneTag);
	}

	DrawList->PopClipRect();
}


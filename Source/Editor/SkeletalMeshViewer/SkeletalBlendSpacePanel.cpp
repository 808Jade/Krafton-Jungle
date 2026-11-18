#include "EnginePCH.h"
#include "Editor/SkeletalMeshViewer/SkeletalBlendSpacePanel.h"
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

void FSkeletalBlendSpacePanel::OnRender()
{
	// 메시가 없거나 이 창을 닫았으면 그리지 않는다. 창의 열림 상태는 탭이 가진다. (탭의 Window 메뉴가 같은 값을 바꾼다)
	if (!Tab.SelectedMesh || !Tab.bBlendSpaceOpen)
	{
		Tab.bBlendSpaceWasVisible = false;
		return;
	}
 if (Tab.Animation.PreviewMode == FSkeletalAnimationPreview::EPreviewMode::StateMachine || Tab.Animation.PreviewMode == FSkeletalAnimationPreview::EPreviewMode::AnimGraph) return;
	ImGui::SetNextWindowClass(&WindowClass);
	const bool Visible = ImGui::Begin(PanelName.c_str(), &Tab.bBlendSpaceOpen, ImGuiWindowFlags_NoNavInputs);
	if (Visible)
	{
		Tab.bBlendSpaceWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (Tab.bBlendSpaceWindowFocused || (!Tab.bBlendSpaceWasVisible && ImGui::IsWindowDocked())) Tab.Animation.ActivatePreviewMode(FSkeletalAnimationPreview::EPreviewMode::BlendSpace);
		const auto Assets = UAssetManager::Get().GetBlendSpaceAssets(Tab.SelectedMesh);
		ImGui::SetNextItemWidth(std::min(ImGui::GetContentRegionAvail().x * .65f, 450.0f));
		if (ImGui::BeginCombo("##PreviewBlendSpace", Tab.Animation.SelectedBlendSpace ? Tab.Animation.SelectedBlendSpace->GetPath().c_str() : "None"))
		{
			if (ImGui::Selectable("None", !Tab.Animation.SelectedBlendSpace)) Tab.Animation.SelectPreviewBlendSpace(nullptr);
			for (auto* Asset : Assets)
				if (ImGui::Selectable(Asset->GetPath().c_str(), Asset == Tab.Animation.SelectedBlendSpace)) Tab.Animation.SelectPreviewBlendSpace(Asset);
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!Tab.Animation.SelectedBlendSpace || !Tab.Animation.OnOpenBlendSpaceEditor);
		if (ImGui::Button("Edit Asset"))
		{
			const FString Path = FFileHelper::ToUtf8Path(fs::path("Assets") / FFileHelper::ToNativePath(Tab.Animation.SelectedBlendSpace->GetPath()));
			Tab.Animation.OnOpenBlendSpaceEditor(Path);
		}
		ImGui::EndDisabled();
		if (!Tab.Animation.SelectedBlendSpace)
		{
			ImGui::TextDisabled(Assets.IsEmpty() ? "No compatible BlendSpace 1D assets. Create one in the Content Drawer." : "Select a BlendSpace 1D asset to preview.");
		}
		else
		{
			const bool Playable = Tab.Animation.IsBlendSpacePlayable();
			if (!Playable && Tab.Animation.BlendSpaceMessage.empty()) Tab.Animation.BlendSpaceMessage = "No playable samples for this mesh.";
			if (Playable) Tab.Animation.BlendSpaceMessage.clear();
			if (!Tab.Animation.BlendSpaceMessage.empty()) ImGui::TextWrapped("%s", Tab.Animation.BlendSpaceMessage.c_str());
			auto* Instance = Tab.Animation.GetPreviewAnimInstance();
			ImGui::BeginDisabled(!Playable || !Instance);
			const auto* Axis = Tab.Animation.SelectedBlendSpace->GetBlendParameter(0);
			if (Axis && std::isfinite(Axis->Min) && std::isfinite(Axis->Max) && Axis->Min < Axis->Max)
			{
				float Input = Tab.Animation.BlendSpaceInput;
				ImGui::SetNextItemWidth(std::min(260.0f, ImGui::GetContentRegionAvail().x*.45f));
				const FString AxisLabel = Axis->Name.ToString() + "###BlendSpacePreviewAxis";
				if (ImGui::SliderFloat(AxisLabel.c_str(), &Input, Axis->Min, Axis->Max, "%.3f", ImGuiSliderFlags_AlwaysClamp)) Tab.Animation.ApplyBlendSpaceInput(Input);
				ImGui::SameLine(); ImGui::SetNextItemWidth(100);
				if (ImGui::InputFloat("##PreviewInput", &Input, 0, 0, "%.4f")) Tab.Animation.ApplyBlendSpaceInput(Input);
			}
			if (DrawTransportButton("##BlendFront", ETransportIcon::ToFirst, false))
			{
				Tab.Animation.ActivatePreviewMode(FSkeletalAnimationPreview::EPreviewMode::BlendSpace);
				Instance->SetPlaying(false); Instance->SetPosition(0); Tab.Animation.RefreshPreviewPose();
			}
			ImGui::SameLine();
			const bool Playing = Tab.Animation.PreviewMode == FSkeletalAnimationPreview::EPreviewMode::BlendSpace && Instance && Instance->IsPlaying();
			if (DrawTransportButton("##BlendPlay", Playing ? ETransportIcon::Pause : ETransportIcon::Play, false))
			{
				Tab.Animation.ActivatePreviewMode(FSkeletalAnimationPreview::EPreviewMode::BlendSpace); Tab.Animation.TogglePreviewPlayback();
			}
			ImGui::SameLine();
			bool Loop = Tab.Animation.PreviewMode == FSkeletalAnimationPreview::EPreviewMode::BlendSpace && Instance ? Instance->IsLooping() : Tab.Animation.BlendSpacePlayback.Looping;
			if (ImGui::Checkbox("Loop", &Loop)) { Tab.Animation.ActivatePreviewMode(FSkeletalAnimationPreview::EPreviewMode::BlendSpace); Instance->SetLooping(Loop); }
			ImGui::SameLine(); ImGui::SetNextItemWidth(80);
			float Rate = Tab.Animation.PreviewMode == FSkeletalAnimationPreview::EPreviewMode::BlendSpace && Instance ? Instance->GetPlayRate() : Tab.Animation.BlendSpacePlayback.Rate;
			if (ImGui::DragFloat("Speed###BlendSpacePlaybackRate", &Rate, .05f, .01f, 10.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp))
			{
				Tab.Animation.ActivatePreviewMode(FSkeletalAnimationPreview::EPreviewMode::BlendSpace); Instance->SetPlayRate(std::clamp(Rate, .01f, 10.0f));
			}
			const bool Active = Tab.Animation.PreviewMode == FSkeletalAnimationPreview::EPreviewMode::BlendSpace && Instance;
			TArray<FBlendSampleWeight> Weights;
			Tab.Animation.SelectedBlendSpace->GetSampleWeights(FVector(Tab.Animation.BlendSpaceInput,0,0), Weights);
			const float Length = Active ? Instance->GetLength() : Tab.Animation.SelectedBlendSpace->GetEffectivePlayLength(Weights);
			float Progress = Active && Length > 0 ? Instance->GetPosition()/Length : Tab.Animation.BlendSpacePlayback.NormalizedTime;
			ImGui::SetNextItemWidth(std::min(400.0f, ImGui::GetContentRegionAvail().x*.6f));
			if (ImGui::SliderFloat("Progress", &Progress, 0, 1, "%.3f", ImGuiSliderFlags_AlwaysClamp))
			{
				Tab.Animation.ActivatePreviewMode(FSkeletalAnimationPreview::EPreviewMode::BlendSpace); Instance->SetPlaying(false);
				Instance->SetPosition(Progress*Instance->GetLength()); Tab.Animation.RefreshPreviewPose();
			}
			ImGui::SameLine(); ImGui::Text("%.2fs / %.2fs", Progress*Length, Length);
			ImGui::EndDisabled();
			DrawBlendSpaceGraph();
		}
	}
	Tab.bBlendSpaceWasVisible = Visible;
	ImGui::End();
}

void FSkeletalBlendSpacePanel::DrawBlendSpaceGraph()
{
	if (!Tab.Animation.SelectedBlendSpace) return;
	const auto* Axis = Tab.Animation.SelectedBlendSpace->GetBlendParameter(0);
	if (!Axis || !std::isfinite(Axis->Min) || !std::isfinite(Axis->Max) || Axis->Min >= Axis->Max) return;
	const ImVec2 Origin = ImGui::GetCursorScreenPos();
	const ImVec2 Size(std::max(ImGui::GetContentRegionAvail().x, 120.0f), std::max(ImGui::GetContentRegionAvail().y - 50.0f, 100.0f));
	ImGui::InvisibleButton("##BlendSpacePreviewGraph", Size);
	const bool Hovered = ImGui::IsItemHovered();
	const float Left = Origin.x + 24, Width = std::max(Size.x - 48, 1.0f), Y = Origin.y + Size.y * .45f;
	const double Range = static_cast<double>(Axis->Max) - Axis->Min;
	auto ScreenX = [&](float X) { return Left + static_cast<float>((static_cast<double>(X) - Axis->Min) / Range) * Width; };
	if (Hovered && ImGui::GetIO().KeyCtrl && Tab.Animation.IsBlendSpacePlayable())
	{
		const double Alpha = std::clamp(static_cast<double>((ImGui::GetMousePos().x - Left) / Width), 0.0, 1.0);
		Tab.Animation.ApplyBlendSpaceInput(static_cast<float>(Axis->Min + Alpha * Range));
	}
	TArray<FBlendSampleWeight> Weights;
	Tab.Animation.SelectedBlendSpace->GetSampleWeights(FVector(Tab.Animation.BlendSpaceInput, 0, 0), Weights);
	auto* Draw = ImGui::GetWindowDrawList();
	Draw->AddRectFilled(Origin, ImVec2(Origin.x + Size.x, Origin.y + Size.y), IM_COL32(25, 27, 30, 255));
	for (int I = 0; I <= 10; ++I)
	{
		const float X = Left + Width * I / 10.0f;
		Draw->AddLine(ImVec2(X, Origin.y + 8), ImVec2(X, Origin.y + Size.y - 28), IM_COL32(55, 57, 62, 255));
		char Label[32]; snprintf(Label, sizeof(Label), "%.3g", Axis->Min + Range * I / 10.0);
		Draw->AddText(ImVec2(X - ImGui::CalcTextSize(Label).x * .5f, Origin.y + Size.y - 22), IM_COL32(180, 183, 190, 255), Label);
	}
	Draw->AddLine(ImVec2(Left, Y), ImVec2(Left + Width, Y), IM_COL32(120, 125, 130, 255));
	const bool Influence = Hovered && ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyAlt;
	for (int I = 0; I < Tab.Animation.SelectedBlendSpace->GetNumSamples(); ++I)
	{
		const auto& Sample = *Tab.Animation.SelectedBlendSpace->GetSample(I);
		const float X = ScreenX(Sample.SampleValue.X);
		float Weight = 0;
		for (const auto& Entry : Weights) if (Entry.SampleIndex == I) Weight = Entry.Weight;
		const ImU32 Color = Influence && Weight > 0 ? IM_COL32(255, 180, 65, 255) : IM_COL32(105, 180, 220, 255);
		Draw->AddQuadFilled(ImVec2(X, Y-6), ImVec2(X+6, Y), ImVec2(X, Y+6), ImVec2(X-6, Y), Color);
		if (Influence && Weight > 0)
		{
			char Label[24]; snprintf(Label, sizeof(Label), "%.1f%%", Weight * 100.0f);
			Draw->AddText(ImVec2(X-16, Y+12), Color, Label);
			Draw->AddLine(ImVec2(X,Y), ImVec2(ScreenX(Tab.Animation.BlendSpaceInput),Y), Color, 2);
		}
		if (Hovered && std::abs(ImGui::GetMousePos().x-X) < 10 && std::abs(ImGui::GetMousePos().y-Y) < 10)
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(Sample.Animation ? Sample.Animation->GetPath().c_str() : "Missing animation");
			ImGui::Text("X: %.6g | Weight: %.1f%%", Sample.SampleValue.X, Weight*100);
			ImGui::EndTooltip();
		}
	}
	const float X = ScreenX(std::clamp(Tab.Animation.BlendSpaceInput, Axis->Min, Axis->Max));
	const ImU32 Green = IM_COL32(80, 235, 120, 255);
	Draw->AddLine(ImVec2(X-9,Y), ImVec2(X+9,Y), Green, 2);
	Draw->AddLine(ImVec2(X,Y-9), ImVec2(X,Y+9), Green, 2);
	ImGui::TextDisabled("Ctrl: preview input | Ctrl+Alt: show sample influences");
	for (const auto& Entry : Weights)
	{
		const auto* Sample = Tab.Animation.SelectedBlendSpace->GetSample(Entry.SampleIndex);
		if (Sample && Sample->Animation) ImGui::Text("%.1f%%  %s", Entry.Weight*100, Sample->Animation->GetPath().c_str());
	}
}


#include "EnginePCH.h"
#include "Editor/SkeletalMeshViewer/SkeletalAnimationPanel.h"
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

void FSkeletalAnimationPanel::OnRender()
{
	// 메시가 없거나 이 창을 닫았으면 그리지 않는다. 창의 열림 상태는 탭이 가진다. (탭의 Window 메뉴가 같은 값을 바꾼다)
	if (!Tab.SelectedMesh || !Tab.bAnimationOpen)
	{
		Tab.bAnimationWasVisible = false;
		return;
	}
 if (Tab.Animation.PreviewMode == FSkeletalAnimationPreview::EPreviewMode::StateMachine || Tab.Animation.PreviewMode == FSkeletalAnimationPreview::EPreviewMode::AnimGraph) return;
	ImGui::SetNextWindowClass(&WindowClass);
	// 키보드 내비게이션을 끈다. 켜 두면 Space가 포커스된 버튼도 눌러 재생 토글이 두 번 일어난다.
	const bool Visible = ImGui::Begin(PanelName.c_str(), &Tab.bAnimationOpen, ImGuiWindowFlags_NoNavInputs);
	if (Visible)
	{
		Tab.bAnimationWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (Tab.bAnimationWindowFocused || (!Tab.bAnimationWasVisible && ImGui::IsWindowDocked())) Tab.Animation.ActivatePreviewMode(FSkeletalAnimationPreview::EPreviewMode::Sequence);

		UAnimSingleNodeInstance* AnimInstance = Tab.Animation.GetPreviewAnimInstance();

		if (!AnimInstance)
		{
			ImGui::TextDisabled("Preview has no single-node anim instance. Clip playback unavailable.");
			ImGui::End();
			return;
		}

		const bool bHasSelection = Tab.Animation.SelectedClipIndex >= 0 && Tab.Animation.SelectedClipIndex < Tab.Animation.PreviewClips.Num();
		const char* PreviewLabel = bHasSelection ? Tab.Animation.PreviewClips[Tab.Animation.SelectedClipIndex].Name.c_str() : "None";

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Clip");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(260.0f);
		if (ImGui::BeginCombo("##PreviewClip", PreviewLabel, ImGuiComboFlags_HeightLargest))
		{
			if (ImGui::IsWindowAppearing())
			{
				ClipSearchFilter[0] = '\0';
				ImGui::SetKeyboardFocusHere();
			}
			ImGui::SetNextItemWidth(-FLT_MIN);
			const bool bSearchSubmitted = ImGui::InputTextWithHint("##ClipSearch", "Search clip...", ClipSearchFilter,
				sizeof(ClipSearchFilter), ImGuiInputTextFlags_EnterReturnsTrue);

			auto ToLower = [](FString Text)
			{
				std::transform(Text.begin(), Text.end(), Text.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
				return Text;
			};
			const FString Filter = ToLower(ClipSearchFilter);

			TArray<int32> Matches;
			for (int32 i = 0; i < Tab.Animation.PreviewClips.Num(); ++i)
			{
				if (Filter.empty() || ToLower(Tab.Animation.PreviewClips[i].Name).find(Filter) != FString::npos)
				{
					Matches.Add(i);
				}
			}
			ImGui::TextDisabled("%d / %d clips", Matches.Num(), Tab.Animation.PreviewClips.Num());

			if (bSearchSubmitted && !Matches.IsEmpty() && IsPlayableClip(Tab.Animation.PreviewClips[Matches[0]].Sequence))
			{
				Tab.Animation.SelectPreviewClip(Matches[0]);
				ImGui::CloseCurrentPopup();
			}

			const float ListHeight = ImGui::GetTextLineHeightWithSpacing() * std::min(Matches.Num() + 1, 14) + ImGui::GetStyle().WindowPadding.y;
			ImGui::BeginChild("##ClipList", ImVec2(0.0f, ListHeight));
			if (Filter.empty() && ImGui::Selectable("None", !bHasSelection) && bHasSelection)
			{
				Tab.Animation.SelectPreviewClip(-1);
			}

			for (const int32 i : Matches)
			{
				const bool bSelected = (i == Tab.Animation.SelectedClipIndex);
				const bool bPlayable = IsPlayableClip(Tab.Animation.PreviewClips[i].Sequence);
				ImGui::PushID(i);
				ImGui::BeginDisabled(!bPlayable);
				if (ImGui::Selectable(Tab.Animation.PreviewClips[i].Name.c_str(), bSelected) && !bSelected)
				{
					Tab.Animation.SelectPreviewClip(i);
				}
				ImGui::EndDisabled();
				if (!bPlayable && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				{
					ImGui::SetTooltip("Clip has no frames.");
				}
				if (bSelected && ImGui::IsWindowAppearing())
				{
					ImGui::SetScrollHereY();
				}
				ImGui::PopID();
			}
			if (Matches.IsEmpty())
			{
				ImGui::TextDisabled("No matching clip");
			}
			ImGui::EndChild();
			ImGui::EndCombo();
		}

		if (Tab.Animation.PreviewMode == FSkeletalAnimationPreview::EPreviewMode::BlendSpace)
		{
			ImGui::TextDisabled("BlendSpace preview is active. Focus this tab to use sequence playback.");
			Tab.bAnimationWasVisible = Visible;
			ImGui::End();
			return;
		}
		// 콤보에서 선택이 바뀌었을 수 있어 다시 확인한다.
		ImGui::SameLine();
		const UAnimSequence* Clip = (Tab.Animation.SelectedClipIndex >= 0 && Tab.Animation.SelectedClipIndex < Tab.Animation.PreviewClips.Num())
			? Tab.Animation.PreviewClips[Tab.Animation.SelectedClipIndex].Sequence
			: nullptr;
		const UAnimDataModel* Model = Clip ? Clip->GetDataModel() : nullptr;
		if (Model)
		{
			ImGui::TextDisabled("%.2fs | %d frames | %.0f fps",
				Model->GetPlayLength(),
				Model->GetNumberOfFrames(),
				Model->GetFrameRate().AsDecimal());
		}
		else
		{
			ImGui::TextDisabled("No animation clip selected (edit mode)");
		}

		if (Model)
		{
			ImGui::SameLine(0.0f, 16.0f);
			const char* SaveLabel = Tab.NotifyEdit.bNotifyDirty ? "Save Notifies *###SaveNotifies" : "Save Notifies###SaveNotifies";
			if (ImGui::Button(SaveLabel))
			{
				Tab.NotifyEdit.SavePreviewNotifies();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Write notifies of this FBX to <fbx>.notify");
			}
		}

		DrawPreviewTimeline(Model);
		DrawNotifyProperties(Model);
		DrawFiredNotifies();

		ImGui::Separator();
		ImGui::BeginDisabled(Model == nullptr);
		{
			const bool bPlaying = Model && AnimInstance->IsPlaying();
			const bool bPlayingForward = bPlaying && Tab.Animation.PreviewPlayDirection > 0;
			const bool bPlayingReverse = bPlaying && Tab.Animation.PreviewPlayDirection < 0;
			const float ClipLength = Model ? Model->GetPlayLength() : 0.0f;

			auto TransportButton = [](const char* Id, ETransportIcon Icon, const char* Tooltip) -> bool
			{
				const bool bPressed = DrawTransportButton(Id, Icon, false);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", Tooltip);
				}
				ImGui::SameLine();
				return bPressed;
			};

			if (TransportButton("##ToFirst", ETransportIcon::ToFirst, "To Front"))
			{
				Tab.Animation.ScrubPreviewToTime(0.0f);
			}
			if (TransportButton("##StepBack", ETransportIcon::StepBack, "Previous Frame (Left Arrow)"))
			{
				Tab.Animation.StepPreviewFrame(-1);
			}
			if (TransportButton("##Reverse", bPlayingReverse ? ETransportIcon::Pause : ETransportIcon::Reverse,
				bPlayingReverse ? "Pause" : "Play Reverse"))
			{
				Tab.Animation.PlayPreviewInDirection(-1);
			}
			if (TransportButton("##PlayPause", bPlayingForward ? ETransportIcon::Pause : ETransportIcon::Play,
				bPlayingForward ? "Pause (Space)" : "Play Forward (Space)"))
			{
				Tab.Animation.PlayPreviewInDirection(+1);
			}
			if (TransportButton("##StepForward", ETransportIcon::StepForward, "Next Frame (Right Arrow)"))
			{
				Tab.Animation.StepPreviewFrame(+1);
			}
			if (TransportButton("##ToLast", ETransportIcon::ToLast, "To End"))
			{
				Tab.Animation.ScrubPreviewToTime(ClipLength);
			}

			ImGui::SameLine(0.0f, 12.0f);
			const bool bLooping = AnimInstance->IsLooping();
			if (DrawTransportButton("##Loop", ETransportIcon::Loop, bLooping))
			{
				AnimInstance->SetLooping(!bLooping);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip(bLooping ? "Looping: On" : "Looping: Off");
			}

			ImGui::SameLine();
			const std::string SpeedLabel = FormatPlaySpeed(Tab.Animation.PreviewPlaySpeed) + "###PlaybackSpeed";
			if (ImGui::Button(SpeedLabel.c_str()))
			{
				ImGui::OpenPopup("##PlaybackSpeedPopup");
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Playback Speed");
			}
			if (ImGui::BeginPopup("##PlaybackSpeedPopup"))
			{
				static constexpr float SpeedPresets[] = { 0.1f, 0.25f, 0.5f, 0.75f, 1.0f, 2.0f, 5.0f, 10.0f };

				ImGui::TextDisabled("Playback Speed");
				ImGui::Separator();

				bool bIsPreset = false;
				for (const float Preset : SpeedPresets)
				{
					bIsPreset |= std::fabs(Tab.Animation.PreviewPlaySpeed - Preset) < 1e-4f;
				}

				for (const float Preset : SpeedPresets)
				{
					const bool bSelected = std::fabs(Tab.Animation.PreviewPlaySpeed - Preset) < 1e-4f;
					if (ImGui::RadioButton(FormatPlaySpeed(Preset).c_str(), bSelected))
					{
						Tab.Animation.SetPreviewPlaySpeed(Preset);
						ImGui::CloseCurrentPopup();
					}
				}
				if (ImGui::RadioButton("xCustom", !bIsPreset))
				{
					Tab.Animation.SetPreviewPlaySpeed(Tab.Animation.PreviewCustomSpeed);
					Tab.Animation.PreviewCustomSpeed = Tab.Animation.PreviewPlaySpeed;
				}

				ImGui::SetNextItemWidth(80.0f);
				ImGui::InputFloat("Custom Speed", &Tab.Animation.PreviewCustomSpeed, 0.0f, 0.0f, "%.2f");
				if (ImGui::IsItemDeactivatedAfterEdit())
				{
					Tab.Animation.SetPreviewPlaySpeed(Tab.Animation.PreviewCustomSpeed);
					Tab.Animation.PreviewCustomSpeed = Tab.Animation.PreviewPlaySpeed;
				}
				ImGui::EndPopup();
			}

			ImGui::SameLine(0.0f, 16.0f);
			ImGui::AlignTextToFramePadding();
			if (Model)
			{
				ImGui::Text("Frame %d / %d    %.2fs / %.2fs",
					Tab.Animation.GetPreviewCurrentFrame(),
					Model->GetNumberOfFrames(),
					AnimInstance->GetPosition(),
					Model->GetPlayLength());
			}
			else
			{
				ImGui::TextUnformatted("Frame - / -    -s / -s");
			}
		}
		ImGui::EndDisabled();

		const char* SkinningText = (Tab.PreviewComponent && Tab.PreviewComponent->UsesGPUSkinning()) ? "Skinning: GPU" : "Skinning: CPU";
		const float SkinningTextW = ImGui::CalcTextSize(SkinningText).x;
		ImGui::SameLine(0.0f, 16.0f);
		const float RightAlignedX = ImGui::GetWindowContentRegionMax().x - SkinningTextW;
		if (RightAlignedX > ImGui::GetCursorPosX())
		{
			ImGui::SetCursorPosX(RightAlignedX);
		}
		ImGui::TextDisabled("%s", SkinningText);
	}
	Tab.bAnimationWasVisible = Visible;
	ImGui::End();
}

void FSkeletalAnimationPanel::DrawPreviewTimeline(const UAnimDataModel* Model)
{
	constexpr float RulerHeight = 18.0f;
	constexpr float TrackHeight = 28.0f;
	constexpr float SidePadding = 10.0f;

	const float Width = ImGui::GetContentRegionAvail().x;
	if (Width <= SidePadding * 2.0f + 1.0f)
	{
		return;
	}

	const ImVec2 Origin = ImGui::GetCursorScreenPos();
	const ImVec2 Size(Width, RulerHeight + TrackHeight);
	const ImVec2 Max(Origin.x + Size.x, Origin.y + Size.y);
	const float RulerBottom = Origin.y + RulerHeight;

	ImGui::InvisibleButton("##PreviewTimeline", Size);
	const bool bHovered = ImGui::IsItemHovered();
	const bool bActive = ImGui::IsItemActive();
	const bool bActivated = ImGui::IsItemActivated();
	const bool bDeactivated = ImGui::IsItemDeactivated();

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(Origin, ImVec2(Max.x, RulerBottom), ImGui::GetColorU32(ImGuiCol_FrameBg));
	DrawList->AddRectFilled(ImVec2(Origin.x, RulerBottom), Max, ImGui::GetColorU32(ImGuiCol_FrameBg, 0.5f));
	DrawList->AddLine(ImVec2(Origin.x, RulerBottom), ImVec2(Max.x, RulerBottom), ImGui::GetColorU32(ImGuiCol_Border));

	UAnimSingleNodeInstance* AnimInstance = Tab.Animation.GetPreviewAnimInstance();
	if (!Model || !AnimInstance || !Tab.Animation.HasPreviewClip())
	{
		return;
	}

	const int32 NumFrames = std::max(Model->GetNumberOfFrames(), 1);
	const float Length = Model->GetPlayLength();
	const float FPS = Model->GetFrameRate().AsDecimal();
	const float X0 = Origin.x + SidePadding;
	const float X1 = Max.x - SidePadding;
	const float Span = X1 - X0;
	const float PixelsPerFrame = Span / static_cast<float>(NumFrames);

	auto FrameToX = [&](float Frame) { return X0 + (Frame / static_cast<float>(NumFrames)) * Span; };
	auto XToTime = [&](float X) { return std::clamp((X - X0) / Span, 0.0f, 1.0f) * Length; };
	auto TimeToX = [&](float Time)
	{
		return X0 + (Length > 0.0f ? std::clamp(Time / Length, 0.0f, 1.0f) : 0.0f) * Span;
	};

	const ImVec2 MousePos = ImGui::GetIO().MousePos;
	const float TrackTop = RulerBottom + 4.0f;
	const float TrackBottom = Max.y - 4.0f;
	const float TrackMidY = (TrackTop + TrackBottom) * 0.5f;
	const bool bMouseInTrack = bHovered && MousePos.y >= TrackTop && MousePos.y <= TrackBottom;
	constexpr float DiamondRadius = 5.0f;

	const UAnimSequence* Sequence = Tab.Animation.GetSelectedPreviewSequence();
	const TArray<FAnimNotifyEvent>* Notifies = Sequence ? &Sequence->Notifies : nullptr;
	const int32 NotifyCount = Notifies ? Notifies->Num() : 0;
	if (Tab.NotifyEdit.SelectedNotifyIndex >= NotifyCount)
	{
		Tab.NotifyEdit.SelectPreviewNotify(-1);
		Tab.NotifyEdit.bDraggingNotify = false;
	}

	int32 HoveredNotify = -1;
	if (bMouseInTrack)
	{
		for (int32 i = NotifyCount - 1; i >= 0 && HoveredNotify < 0; --i)
		{
			const FAnimNotifyEvent& Notify = (*Notifies)[i];
			const float XStart = TimeToX(Notify.TriggerTime);
			if (IsNotifyStateEvent(Notify))
			{
				const float XEnd = std::max(TimeToX(Notify.TriggerTime + Notify.Duration), XStart + 2.0f);
				if (MousePos.x >= XStart - DiamondRadius && MousePos.x <= XEnd)
				{
					HoveredNotify = i;
				}
			}
			else if (std::fabs(MousePos.x - XStart) <= DiamondRadius + 1.0f)
			{
				HoveredNotify = i;
			}
		}
	}

	if (bActivated)
	{
		if (HoveredNotify >= 0)
		{
			Tab.NotifyEdit.SelectPreviewNotify(HoveredNotify);
			Tab.NotifyEdit.bDraggingNotify = true;
			Tab.NotifyEdit.NotifyDragOffset = XToTime(MousePos.x) - (*Notifies)[HoveredNotify].TriggerTime;
		}
		else
		{
			Tab.NotifyEdit.SelectPreviewNotify(-1);
			Tab.NotifyEdit.bDraggingNotify = false;
		}
	}

	if (bActive)
	{
		if (Tab.NotifyEdit.bDraggingNotify && Tab.NotifyEdit.SelectedNotifyIndex >= 0)
		{
			Tab.NotifyEdit.SetPreviewNotifyTime(Tab.NotifyEdit.SelectedNotifyIndex, XToTime(MousePos.x) - Tab.NotifyEdit.NotifyDragOffset, !ImGui::GetIO().KeyShift);
		}
		else
		{
			Tab.Animation.ScrubPreviewToTime(XToTime(MousePos.x));
		}
	}

	if (bDeactivated && Tab.NotifyEdit.bDraggingNotify)
	{
		Tab.NotifyEdit.bDraggingNotify = false;
		if (Tab.NotifyEdit.SelectedNotifyIndex >= 0)
		{
			Tab.NotifyEdit.SelectPreviewNotify(Tab.NotifyEdit.SortPreviewNotifies(Tab.NotifyEdit.SelectedNotifyIndex));
		}
	}

	if (bMouseInTrack && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		Tab.NotifyEdit.NotifyPopupIndex = HoveredNotify;
		Tab.NotifyEdit.NotifyPopupTime = XToTime(MousePos.x);
		if (HoveredNotify >= 0)
		{
			Tab.NotifyEdit.SelectPreviewNotify(HoveredNotify);
		}
		ImGui::OpenPopup("##NotifyContext");
	}

	const float MinLabelSpacing = ImGui::CalcTextSize(std::to_string(NumFrames).c_str()).x + 12.0f;
	int32 MajorStep = 1;
	for (const int32 Candidate : { 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000 })
	{
		MajorStep = Candidate;
		if (Candidate * PixelsPerFrame >= MinLabelSpacing)
		{
			break;
		}
	}
	int32 MinorStep = (MajorStep % 5 == 0) ? MajorStep / 5 : ((MajorStep % 2 == 0) ? MajorStep / 2 : MajorStep);
	if (MinorStep * PixelsPerFrame < 4.0f)
	{
		MinorStep = MajorStep;
	}

	const ImU32 TickCol = ImGui::GetColorU32(ImGuiCol_TextDisabled);
	const ImU32 LabelCol = ImGui::GetColorU32(ImGuiCol_Text);
	const ImU32 GridCol = ImGui::GetColorU32(ImGuiCol_Border, 0.6f);
	for (int32 Frame = 0; Frame <= NumFrames; Frame += MinorStep)
	{
		const float X = FrameToX(static_cast<float>(Frame));
		if (Frame % MajorStep == 0)
		{
			DrawList->AddLine(ImVec2(X, RulerBottom - 7.0f), ImVec2(X, RulerBottom), TickCol);
			DrawList->AddLine(ImVec2(X, RulerBottom), ImVec2(X, Max.y), GridCol);
			DrawList->AddText(ImVec2(X + 3.0f, Origin.y + 1.0f), LabelCol, std::to_string(Frame).c_str());
		}
		else
		{
			DrawList->AddLine(ImVec2(X, RulerBottom - 3.0f), ImVec2(X, RulerBottom), TickCol);
		}
	}
	DrawList->AddLine(ImVec2(X1, Origin.y), ImVec2(X1, Max.y), TickCol);

	const float CurrentTime = AnimInstance->GetPosition();
	auto DrawDiamond = [&](float X, ImU32 Col)
	{
		DrawList->AddQuadFilled(
			ImVec2(X, TrackMidY - DiamondRadius), ImVec2(X + DiamondRadius, TrackMidY),
			ImVec2(X, TrackMidY + DiamondRadius), ImVec2(X - DiamondRadius, TrackMidY), Col);
	};
	auto DrawDiamondOutline = [&](float X, ImU32 Col)
	{
		const float R = DiamondRadius + 2.0f;
		DrawList->AddQuad(
			ImVec2(X, TrackMidY - R), ImVec2(X + R, TrackMidY),
			ImVec2(X, TrackMidY + R), ImVec2(X - R, TrackMidY), Col, 2.0f);
	};

	const ImU32 PointCol = IM_COL32(235, 200, 60, 255);
	const ImU32 RangeCol = IM_COL32(70, 150, 220, 170);
	const ImU32 RangeActiveCol = IM_COL32(120, 200, 255, 240);
	const ImU32 NotifyTextCol = IM_COL32(15, 15, 20, 255);
	const ImU32 SelectedCol = IM_COL32(255, 255, 255, 255);
	const ImU32 FiredCol = IM_COL32(255, 120, 60, 255);

	DrawList->PushClipRect(Origin, Max, true);
	for (int32 i = 0; i < NotifyCount; ++i)
	{
		const FAnimNotifyEvent& Notify = (*Notifies)[i];
		const FString NotifyName = GetNotifyLabel(Notify);
		const float XStart = TimeToX(Notify.TriggerTime);
		const ImVec2 NameSize = ImGui::CalcTextSize(NotifyName.c_str());
		const bool bSelected = (i == Tab.NotifyEdit.SelectedNotifyIndex);

		if (IsNotifyStateEvent(Notify))
		{
			const float EndTime = Notify.TriggerTime + Notify.Duration;
			const float XEnd = std::max(TimeToX(EndTime), XStart + 2.0f);
			const bool bActiveNow = (CurrentTime >= Notify.TriggerTime && CurrentTime <= EndTime) || Tab.Animation.IsPreviewNotifyFlashing(Notify);

			DrawList->AddRectFilled(ImVec2(XStart, TrackTop), ImVec2(XEnd, TrackBottom),
				bActiveNow ? RangeActiveCol : RangeCol, 4.0f);
			if (bSelected)
			{
				DrawList->AddRect(ImVec2(XStart, TrackTop), ImVec2(XEnd, TrackBottom), SelectedCol, 4.0f, 0, 2.0f);
			}
			DrawDiamond(XStart, PointCol);
			if (NameSize.x < XEnd - XStart - DiamondRadius * 2.0f - 4.0f)
			{
				DrawList->AddText(ImVec2(XStart + DiamondRadius + 4.0f, TrackMidY - NameSize.y * 0.5f),
					NotifyTextCol, NotifyName.c_str());
			}
		}
		else
		{
			const ImU32 MarkerCol = Tab.Animation.IsPreviewNotifyFlashing(Notify) ? FiredCol : PointCol;
			DrawList->AddLine(ImVec2(XStart, TrackTop), ImVec2(XStart, TrackBottom), MarkerCol);
			DrawDiamond(XStart, MarkerCol);
			if (bSelected)
			{
				DrawDiamondOutline(XStart, SelectedCol);
			}
			DrawList->AddText(ImVec2(XStart + DiamondRadius + 3.0f, TrackMidY - NameSize.y * 0.5f),
				LabelCol, NotifyName.c_str());
		}
	}
	DrawList->PopClipRect();

	const ImU32 PlayheadCol = IM_COL32(230, 60, 50, 255);
	const float PlayheadX = FrameToX(AnimInstance->GetPosition() * FPS);
	DrawList->AddLine(ImVec2(PlayheadX, Origin.y), ImVec2(PlayheadX, Max.y), PlayheadCol, 2.0f);

	const std::string FrameLabel = std::to_string(Tab.Animation.GetPreviewCurrentFrame());
	const ImVec2 LabelSize = ImGui::CalcTextSize(FrameLabel.c_str());
	const float HandleW = LabelSize.x + 8.0f;
	const float HandleX = std::clamp(PlayheadX - HandleW * 0.5f, Origin.x, Max.x - HandleW);
	DrawList->AddRectFilled(ImVec2(HandleX, Origin.y), ImVec2(HandleX + HandleW, RulerBottom), PlayheadCol, 3.0f);
	DrawList->AddText(ImVec2(HandleX + 4.0f, Origin.y + (RulerHeight - LabelSize.y) * 0.5f), IM_COL32(255, 255, 255, 255), FrameLabel.c_str());

	if (bHovered && !bActive)
	{
		if (HoveredNotify >= 0)
		{
			const FAnimNotifyEvent& Notify = (*Notifies)[HoveredNotify];
			const FString NotifyName = GetNotifyLabel(Notify);
			const int32 StartFrame = static_cast<int32>(std::floor(Notify.TriggerTime * FPS));
			if (IsNotifyStateEvent(Notify))
			{
				ImGui::SetTooltip("%s\nStart %.2fs (frame %d)\nDuration %.2fs",
					NotifyName.c_str(), Notify.TriggerTime, StartFrame, Notify.Duration);
			}
			else
			{
				ImGui::SetTooltip("%s\nTime %.2fs (frame %d)", NotifyName.c_str(), Notify.TriggerTime, StartFrame);
			}
		}
		else
		{
			const float HoverTime = XToTime(MousePos.x);
			const int32 HoverFrame = std::clamp(static_cast<int32>(std::floor(HoverTime * FPS)), 0, NumFrames);
			ImGui::SetTooltip("Frame %d  (%.2fs)", HoverFrame, HoverTime);
		}
	}

	if (ImGui::BeginPopup("##NotifyContext"))
	{
		if (Tab.NotifyEdit.NotifyPopupIndex >= 0)
		{
			if (ImGui::MenuItem("Delete Notify"))
			{
				Tab.NotifyEdit.RemovePreviewNotify(Tab.NotifyEdit.NotifyPopupIndex);
				Tab.NotifyEdit.SelectPreviewNotify(-1);
			}
		}
		else
		{
			if (ImGui::MenuItem("Add Notify"))
			{
				Tab.NotifyEdit.SelectPreviewNotify(Tab.NotifyEdit.AddPreviewNotify(Tab.NotifyEdit.NotifyPopupTime, 0.0f));
			}
			if (ImGui::MenuItem("Add Notify State"))
			{
				Tab.NotifyEdit.SelectPreviewNotify(Tab.NotifyEdit.AddPreviewNotify(Tab.NotifyEdit.NotifyPopupTime, 0.2f));
			}
		}
		ImGui::EndPopup();
	}
}

void FSkeletalAnimationPanel::DrawNotifyProperties(const UAnimDataModel* Model)
{
	const UAnimSequence* Sequence = Tab.Animation.GetSelectedPreviewSequence();
	if (!Model || !Sequence)
	{
		return;
	}

	const int32 NotifyCount = Sequence->Notifies.Num();
	if (Tab.NotifyEdit.SelectedNotifyIndex < 0 || Tab.NotifyEdit.SelectedNotifyIndex >= NotifyCount)
	{
		ImGui::TextDisabled("Notifies: %d    Right-click the timeline to add a notify.", NotifyCount);
		return;
	}

	const float Length = Model->GetPlayLength();
	const float FPS = Model->GetFrameRate().AsDecimal();

	ImGui::PushID("##NotifyProperties");

	UAnimNotify* NotifyObject = Sequence->Notifies[Tab.NotifyEdit.SelectedNotifyIndex].Notify;

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Notify");
	ImGui::SameLine();
	const FString TypeLabel = NotifyObject ? UAnimNotify::GetDisplayNameForClass(NotifyObject->GetClass()) : FString("Name Notify");

	TArray<UClass*> NotifyClasses;
	GetDerivedClasses(UAnimNotify::StaticClass(), NotifyClasses);
	float TypeTextWidth = ImGui::CalcTextSize("Name Notify").x;
	for (const UClass* NotifyClass : NotifyClasses)
	{
		TypeTextWidth = std::max(TypeTextWidth, ImGui::CalcTextSize(UAnimNotify::GetDisplayNameForClass(NotifyClass).c_str()).x);
	}
	ImGui::SetNextItemWidth(TypeTextWidth + ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2.0f);
	if (ImGui::BeginCombo("##NotifyType", TypeLabel.c_str()))
	{
		if (ImGui::Selectable("Name Notify", NotifyObject == nullptr) && NotifyObject)
		{
			Tab.NotifyEdit.SetPreviewNotifyClass(Tab.NotifyEdit.SelectedNotifyIndex, nullptr);
		}

		const bool bIsState = IsNotifyStateEvent(Sequence->Notifies[Tab.NotifyEdit.SelectedNotifyIndex]);
		for (UClass* NotifyClass : NotifyClasses)
		{
			if (!NotifyClass->CanCreateInstance())
			{
				continue;
			}

			const bool bSelected = NotifyObject && NotifyObject->GetClass() == NotifyClass;
			const FString Label = UAnimNotify::GetDisplayNameForClass(NotifyClass);
			const bool bDisabled = bIsState && !IsNotifyStateClass(NotifyClass);
			ImGui::BeginDisabled(bDisabled);
			if (ImGui::Selectable(Label.c_str(), bSelected) && !bSelected)
			{
				Tab.NotifyEdit.SetPreviewNotifyClass(Tab.NotifyEdit.SelectedNotifyIndex, NotifyClass);
			}
			ImGui::EndDisabled();
			if (bDisabled && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			{
				ImGui::SetTooltip("Notify State supports Name Notify or NotifyState classes. Set Duration to 0 first.");
			}
		}
		ImGui::EndCombo();
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Notify type");
	}

	if (Tab.NotifyEdit.SelectedNotifyIndex >= 0 && Tab.NotifyEdit.SelectedNotifyIndex < Sequence->Notifies.Num())
	{
		NotifyObject = Sequence->Notifies[Tab.NotifyEdit.SelectedNotifyIndex].Notify;
	}

	if (!NotifyObject)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(150.0f);
		ImGui::InputText("##NotifyName", Tab.NotifyEdit.NotifyNameBuffer, sizeof(Tab.NotifyEdit.NotifyNameBuffer));
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			Tab.NotifyEdit.RenamePreviewNotify(Tab.NotifyEdit.SelectedNotifyIndex, Tab.NotifyEdit.NotifyNameBuffer);
			Tab.NotifyEdit.SelectPreviewNotify(Tab.NotifyEdit.SelectedNotifyIndex);
		}

		ImGui::SameLine();
		if (ImGui::BeginCombo("##NotifyPreset", nullptr, ImGuiComboFlags_NoPreview))
		{
			const TArray<FString> UsedNames = Tab.NotifyEdit.CollectUsedNotifyNames();
			if (UsedNames.IsEmpty())
			{
				ImGui::TextDisabled("No names used yet");
			}
			for (const FString& UsedName : UsedNames)
			{
				if (ImGui::Selectable(UsedName.c_str(), UsedName == Tab.NotifyEdit.NotifyNameBuffer))
				{
					Tab.NotifyEdit.RenamePreviewNotify(Tab.NotifyEdit.SelectedNotifyIndex, UsedName);
					Tab.NotifyEdit.SelectPreviewNotify(Tab.NotifyEdit.SelectedNotifyIndex);
				}
			}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Names already used in this FBX");
		}
	}

	if (Tab.NotifyEdit.SelectedNotifyIndex >= 0 && Tab.NotifyEdit.SelectedNotifyIndex < Sequence->Notifies.Num())
	{
		ImGui::SameLine(0.0f, 16.0f);
		ImGui::TextUnformatted("Time");
		ImGui::SameLine();
		float Time = Sequence->Notifies[Tab.NotifyEdit.SelectedNotifyIndex].TriggerTime;
		ImGui::SetNextItemWidth(90.0f);
		if (ImGui::DragFloat("##NotifyTime", &Time, 0.01f, 0.0f, Length, "%.3fs"))
		{
			Tab.NotifyEdit.SetPreviewNotifyTime(Tab.NotifyEdit.SelectedNotifyIndex, Time, false);
		}
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			Tab.NotifyEdit.SelectPreviewNotify(Tab.NotifyEdit.SortPreviewNotifies(Tab.NotifyEdit.SelectedNotifyIndex));
		}
		ImGui::SameLine();
		ImGui::TextDisabled("frame %d", static_cast<int32>(std::floor(Time * FPS)));
	}

	if (Tab.NotifyEdit.SelectedNotifyIndex >= 0 && Tab.NotifyEdit.SelectedNotifyIndex < Sequence->Notifies.Num())
	{
		const FAnimNotifyEvent& Current = Sequence->Notifies[Tab.NotifyEdit.SelectedNotifyIndex];
		if (CanHaveNotifyDuration(Current))
		{
			ImGui::SameLine(0.0f, 16.0f);
			ImGui::TextUnformatted("Duration");
			ImGui::SameLine();
			float Duration = Current.Duration;
			ImGui::SetNextItemWidth(90.0f);
			if (ImGui::DragFloat("##NotifyDuration", &Duration, 0.01f, 0.0f, std::max(0.0f, Length - Current.TriggerTime), "%.3fs"))
			{
				Tab.NotifyEdit.SetPreviewNotifyDuration(Tab.NotifyEdit.SelectedNotifyIndex, Duration);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("0 = single notify, > 0 = notify state");
			}
		}

		ImGui::SameLine(0.0f, 16.0f);
		if (ImGui::Button("Delete"))
		{
			Tab.NotifyEdit.RemovePreviewNotify(Tab.NotifyEdit.SelectedNotifyIndex);
			Tab.NotifyEdit.SelectPreviewNotify(-1);
		}
	}

	if (Tab.NotifyEdit.SelectedNotifyIndex >= 0 && Tab.NotifyEdit.SelectedNotifyIndex < Sequence->Notifies.Num() && Sequence->Notifies[Tab.NotifyEdit.SelectedNotifyIndex].Notify)
	{
		DrawNotifyObjectProperties(Tab.NotifyEdit.SelectedNotifyIndex, Sequence->Notifies[Tab.NotifyEdit.SelectedNotifyIndex].Notify);
	}

	ImGui::PopID();
}

void FSkeletalAnimationPanel::DrawNotifyObjectProperties(int32 Index, UAnimNotify* NotifyObject)
{
	ImGui::Indent(ImGui::GetFontSize());
	bool bFirst = true;
	for (const UClass* Class = NotifyObject->GetClass(); Class; Class = Class->Super)
	{
		for (const FProperty& Property : Class->GetProperties())
		{
			if (!Property.HasAnyFlags(CPF_Edit))
			{
				continue;
			}

			const FString Name = Property.Name.ToString();
			void* Ptr = reinterpret_cast<uint8*>(NotifyObject) + Property.Offset;
			const bool bReadOnly = Property.HasAnyFlags(CPF_EditConst);

			if (!bFirst)
			{
				ImGui::SameLine(0.0f, 16.0f);
			}
			bFirst = false;

			ImGui::PushID(Name.c_str());
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(Name.c_str());
			ImGui::SameLine();
			ImGui::BeginDisabled(bReadOnly);

			bool bChanged = false;
			switch (Property.Type)
			{
			case EPropertyType::Float:
				ImGui::SetNextItemWidth(70.0f);
				bChanged = ImGui::DragFloat("##Value", static_cast<float*>(Ptr), 0.01f, 0.0f, 0.0f, "%.2f");
				break;
			case EPropertyType::Int:
				ImGui::SetNextItemWidth(70.0f);
				bChanged = ImGui::DragInt("##Value", static_cast<int32*>(Ptr));
				break;
			case EPropertyType::Bool:
				bChanged = ImGui::Checkbox("##Value", static_cast<bool*>(Ptr));
				break;
			case EPropertyType::String:
			{
				FString& Value = *static_cast<FString*>(Ptr);
				if (const char* AssetFolder = NotifyObject->GetPropertyAssetFolder(Property))
				{
					bChanged = DrawAudioFilePicker(AssetFolder, Value);
					break;
				}

				char Buffer[260];
				snprintf(Buffer, sizeof(Buffer), "%s", Value.c_str());
				ImGui::SetNextItemWidth(240.0f);
				ImGui::InputText("##Value", Buffer, sizeof(Buffer));
				if (ImGui::IsItemDeactivatedAfterEdit())
				{
					Value = Buffer;
					bChanged = true;
				}
				break;
			}
			default:
				ImGui::TextDisabled("(unsupported)");
				break;
			}

			ImGui::EndDisabled();
			ImGui::PopID();

			if (bChanged)
			{
				Tab.NotifyEdit.OnPreviewNotifyPropertyChanged(Index, Property);
			}
		}
	}
	ImGui::Unindent(ImGui::GetFontSize());
}

void FSkeletalAnimationPanel::DrawFiredNotifies()
{
	if (!Tab.Animation.HasPreviewClip())
	{
		return;
	}

	ImGui::Checkbox("Preview Camera Shake", &Tab.Animation.bPreviewCameraShake);
	ImGui::SameLine(0.0f, 16.0f);
	ImGui::TextDisabled("Fired:");
	for (int32 i = Tab.Animation.FiredNotifies.Num() - 1; i >= 0; --i)
	{
		const FSkeletalAnimationPreview::FFiredPreviewNotify& Fired = Tab.Animation.FiredNotifies[i];
		ImGui::SameLine();
		ImGui::Text("%s (%.2fs)", Fired.Label.c_str(), Fired.Event.TriggerTime);
	}
}

bool FSkeletalAnimationPanel::DrawAudioFilePicker(const char* Folder, FString& InOutPath)
{
	bool bChanged = false;
	const FString Preview = InOutPath.empty() ? FString("None") : FFileHelper::ToUtf8Path(FFileHelper::ToNativePath(InOutPath).filename());

	ImGui::SetNextItemWidth(220.0f);
	if (ImGui::BeginCombo("##AudioFile", Preview.c_str()))
	{
		if (ImGui::IsWindowAppearing())
		{
			Tab.NotifyEdit.RefreshAudioFileList(Folder);
		}

		if (ImGui::Selectable("None", InOutPath.empty()) && !InOutPath.empty())
		{
			InOutPath.clear();
			bChanged = true;
		}
		if (Tab.NotifyEdit.AudioFileList.IsEmpty())
		{
			ImGui::TextDisabled("No audio files in %s", Folder);
		}
		for (const FString& File : Tab.NotifyEdit.AudioFileList)
		{
			const FString Label = FFileHelper::ToUtf8Path(FFileHelper::ToNativePath(File).lexically_relative(FFileHelper::ToNativePath(Folder)));
			const bool bSelected = File == InOutPath;
			if (ImGui::Selectable(Label.c_str(), bSelected) && !bSelected)
			{
				InOutPath = File;
				bChanged = true;
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	if (!InOutPath.empty() && ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", InOutPath.c_str());
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(InOutPath.empty());
	if (ImGui::SmallButton("Play"))
	{
		if (FAudioSystem* Audio = GEngine ? GEngine->GetAudioSystem() : nullptr)
		{
			Audio->Play(InOutPath, false, 1.0f);
		}
	}
	ImGui::EndDisabled();

	return bChanged;
}


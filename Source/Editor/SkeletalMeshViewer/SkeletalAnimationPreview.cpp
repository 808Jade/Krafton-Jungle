#include "EnginePCH.h"
#include "Editor/SkeletalMeshViewer/SkeletalAnimationPreview.h"
#include "Editor/SkeletalMeshViewer/SkeletalPreviewTabBase.h"
#include "Editor/SkeletalMeshViewer/SkeletalMeshEditorUtils.h"
#include "Animation/AssetAnimInstance.h"
#include "Animation/AnimGraph.h"
#include "SkeletalMeshEditorUtils.h"
#include "Asset/AssetManager.h"
#include "Camera/MinimalViewInfo.h"
#include "Collision/Ray.h"
#include "Engine/SceneManagement.h"
#include "GameFramework/Actor/SkeletalMeshActor.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "GameFramework/Actor/DirectionalLightActor.h"
#include "GameFramework/Actor/SkyLightActor.h"
#include "Engine/World.h"
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

using namespace SkeletalMeshEditorUtils;

void FSkeletalAnimationPreview::HandlePreviewNotify(const FAnimNotifyEvent& Event, UAnimSequenceBase* Sequence)
{
	AddFiredPreviewNotify(Event, GetNotifyLabel(Event));

	if (bPreviewCameraShake)
	{
		if (const UAnimNotify_CameraShake* Shake = Cast<UAnimNotify_CameraShake>(Event.Notify))
		{
			Tab.Camera.PreviewShake = Shake->CreateShakePattern();
			Tab.Camera.PreviewShakeScale = Shake->ShakeScale;
			if (Tab.Camera.PreviewShake)
			{
				Tab.Camera.PreviewShake->StartShakePattern();
			}
		}
	}
}

void FSkeletalAnimationPreview::AddFiredPreviewNotify(const FAnimNotifyEvent& Event, const FString& Label)
{
	constexpr int32 MaxFiredNotifies = 8;

	FFiredPreviewNotify& Fired = FiredNotifies.AddDefaulted_GetRef();
	Fired.Event = Event;
	Fired.Label = Label;
	Fired.FiredAt = ImGui::GetTime();
	while (FiredNotifies.Num() > MaxFiredNotifies)
	{
		FiredNotifies.RemoveAt(0);
	}
}

void FSkeletalAnimationPreview::HandlePreviewNotifyStateBegin(const FAnimNotifyEvent& Event, UAnimSequenceBase* Sequence)
{
	AddFiredPreviewNotify(Event, GetNotifyLabel(Event) + " [Begin]");
}

void FSkeletalAnimationPreview::HandlePreviewNotifyStateEnd(const FAnimNotifyEvent& Event, UAnimSequenceBase* Sequence)
{
	AddFiredPreviewNotify(Event, GetNotifyLabel(Event) + " [End]");
}

bool FSkeletalAnimationPreview::IsPreviewNotifyFlashing(const FAnimNotifyEvent& Event) const
{
	constexpr double FlashSeconds = 0.3;
	const double Now = ImGui::GetTime();
	for (const FFiredPreviewNotify& Fired : FiredNotifies)
	{
		if (Now - Fired.FiredAt <= FlashSeconds &&
			Fired.Event.Notify == Event.Notify &&
			Fired.Event.TriggerTime == Event.TriggerTime &&
			Fired.Event.NotifyName == Event.NotifyName)
		{
			return true;
		}
	}
	return false;
}

UAnimSequence* FSkeletalAnimationPreview::GetSelectedPreviewSequence() const
{
	if (SelectedClipIndex < 0 || SelectedClipIndex >= PreviewClips.Num())
	{
		return nullptr;
	}
	return PreviewClips[SelectedClipIndex].Sequence;
}

UAnimSingleNodeInstance* FSkeletalAnimationPreview::GetPreviewAnimInstance() const
{
	if (!Tab.PreviewComponent)
	{
		return (nullptr);
	}

	return (Tab.PreviewComponent->GetSingleNodeInstance());
}

void FSkeletalAnimationPreview::ClearPreviewClips()
{
	// 클립을 지우기 전에 인스턴스에서 먼저 떼어 낸다.
	if (UAnimSingleNodeInstance* AnimInstance = GetPreviewAnimInstance())
	{
		AnimInstance->SetAnimationAsset(nullptr);
	}

	// 클립은 에셋 매니저 소유이므로 목록만 비운다.
	PreviewClips.Empty();
}

void FSkeletalAnimationPreview::BuildPreviewClips()
{
	ClearPreviewClips();
	Tab.NotifyEdit.bNotifyDirty = false;

	if (!Tab.SelectedMesh)
	{
		return;
	}

	if (const FFbxAssetSet* Assets = UAssetManager::Get().FindFbxAssets(Tab.SelectedMesh->GetPath()))
	{
		for (UAnimSequence* Sequence : Assets->Animations)
		{
			if (!Sequence)
			{
				continue;
			}

			FPreviewClip& Clip = PreviewClips.AddDefaulted_GetRef();
			Clip.Sequence = Sequence;
			Clip.Name = Sequence->GetName();
			if (Clip.Name.empty() || Clip.Name == "None")
			{
				Clip.Name = "Clip " + std::to_string(PreviewClips.Num() - 1);
			}
		}
	}
}

void FSkeletalAnimationPreview::SelectPreviewClip(int32 ClipIndex)
{
	CapturePreviewPlayback();
	SelectedClipIndex = ClipIndex >= 0 && ClipIndex < PreviewClips.Num() &&
		IsPlayableClip(PreviewClips[ClipIndex].Sequence) ? ClipIndex : -1;
	SequencePlayback = FPreviewPlaybackState();
	SequencePlayback.Rate = PreviewPlaySpeed;
	if (auto* Instance = GetPreviewAnimInstance())
	{
		Instance->SetAnimationAsset(nullptr);
		Instance->SetPlaying(false);
	}
	// Force reapplication even when replacing a clip within the same mode.
	PreviewMode = EPreviewMode::None;
	ActivatePreviewMode(EPreviewMode::Sequence);
	if (SelectedClipIndex < 0 && Tab.PreviewComponent)
	{
		Tab.PreviewComponent->ResetPoseToBindPose();
		Tab.PreviewComponent->FlushPendingPose();
	}
	FiredNotifies.Empty();
	Tab.Camera.PreviewShake.reset();
	Tab.NotifyEdit.SelectedNotifyIndex = -1;
	Tab.NotifyEdit.bDraggingNotify = false;
	Tab.NotifyEdit.NotifyNameBuffer[0] = '\0';
}

bool FSkeletalAnimationPreview::HasPreviewClip() const
{
	return PreviewMode == EPreviewMode::Sequence && SelectedClipIndex >= 0 && SelectedClipIndex < PreviewClips.Num();
}

bool FSkeletalAnimationPreview::BlockEditDuringAnimationPreview()
{
	if (!HasActivePreview())
	{
		return (false);
	}

	Tab.SaveStatusMessage = "Animation preview: select None in the active playback tab to edit or save.";
	Tab.SaveStatusTimer = 3.5f;
	return (true);
}

bool FSkeletalAnimationPreview::IsPreviewAtEnd() const
{
	UAnimSingleNodeInstance* AnimSingleNodeInstance = GetPreviewAnimInstance();
	if (!AnimSingleNodeInstance || !HasActivePreview())
	{
		return (false);
	}

	if (AnimSingleNodeInstance->GetPlayRate() >= 0.0f)
	{
		return (AnimSingleNodeInstance->GetPosition() >= AnimSingleNodeInstance->GetLength() - 1e-4f);
	}
	return (AnimSingleNodeInstance->GetPosition() <= 1e-4f);
}

void FSkeletalAnimationPreview::UpdatePreviewPlaybackEnd()
{
	UAnimSingleNodeInstance* AnimSingleNodeInstance = GetPreviewAnimInstance();
	if (!AnimSingleNodeInstance || !HasActivePreview() || !AnimSingleNodeInstance->IsPlaying() || AnimSingleNodeInstance->IsLooping() || !IsPreviewAtEnd())
	{
		return;
	}
	AnimSingleNodeInstance->SetPlaying(false);
}

void FSkeletalAnimationPreview::TogglePreviewPlayback() 
{
	UAnimSingleNodeInstance* AnimSingleNodeInstance = GetPreviewAnimInstance();
	if (!AnimSingleNodeInstance || !HasActivePreview())
	{
		return;
	}

	if (AnimSingleNodeInstance->IsPlaying())
	{
		AnimSingleNodeInstance->SetPlaying(false);
		return;
	}

	if (!AnimSingleNodeInstance->IsLooping() && IsPreviewAtEnd())
	{
		AnimSingleNodeInstance->SetPosition(AnimSingleNodeInstance->GetPlayRate() >= 0.0f ? 0.0f : AnimSingleNodeInstance->GetLength());
	}
	AnimSingleNodeInstance->SetPlaying(true);
}

int32 FSkeletalAnimationPreview::GetPreviewCurrentFrame() const
{
	UAnimSingleNodeInstance* AnimSingleNodeInstance = GetPreviewAnimInstance();
	if (!AnimSingleNodeInstance || !HasPreviewClip())
	{
		return (0);
	}

	const UAnimDataModel* Model = PreviewClips[SelectedClipIndex].Sequence->GetDataModel();
	const float ClipFPS = Model->GetFrameRate().AsDecimal();
	// 정규화 시간 왕복 오차(29 → 28.99999)로 한 프레임 낮게 내림되지 않도록 여유를 둔다.
	const int32 Frame = static_cast<int32>(std::floor(AnimSingleNodeInstance->GetPosition() * ClipFPS + 1e-3f));
	return (std::clamp(Frame, 0, Model->GetNumberOfFrames()));
}

void FSkeletalAnimationPreview::ScrubPreviewToTime(float Seconds)
{
	UAnimSingleNodeInstance* AnimSingleNodeInstance = GetPreviewAnimInstance();
	if (!AnimSingleNodeInstance || !HasActivePreview())
	{
		return;
	}

	AnimSingleNodeInstance->SetPlaying(false);
	AnimSingleNodeInstance->SetPosition(Seconds);
	Tab.PreviewComponent->RefreshAnimationPose();
}

void FSkeletalAnimationPreview::ApplyPreviewPlayRate()
{
	UAnimSingleNodeInstance* AnimSingleNodeInstance = GetPreviewAnimInstance();
	if (!AnimSingleNodeInstance)
	{
		return;
	}
	AnimSingleNodeInstance->SetPlayRate(static_cast<float>(PreviewPlayDirection) * PreviewPlaySpeed);
}

void FSkeletalAnimationPreview::SetPreviewPlaySpeed(float Speed)
{
	PreviewPlaySpeed = std::clamp(Speed, 0.01f, 100.0f);
	ApplyPreviewPlayRate();
}

void FSkeletalAnimationPreview::HandlePreviewShortcuts()
{
	const ImGuiIO& IO = ImGui::GetIO();
	if (!HasActivePreview() || IO.WantTextInput || IO.KeyCtrl || IO.KeyAlt ||
		ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) ||
		ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle) || Tab.bIsInteracting) return;
	if (!Tab.bViewportFocused && !(PreviewMode == EPreviewMode::Sequence ? Tab.bAnimationWindowFocused : Tab.bBlendSpaceWindowFocused)) return;
	if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) TogglePreviewPlayback();
	if (HasPreviewClip())
	{
		if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) StepPreviewFrame(-1);
		if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) StepPreviewFrame(+1);
		if (Tab.bAnimationWindowFocused && Tab.NotifyEdit.SelectedNotifyIndex >= 0 && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
		{
			Tab.NotifyEdit.RemovePreviewNotify(Tab.NotifyEdit.SelectedNotifyIndex);
			Tab.NotifyEdit.SelectPreviewNotify(-1);
		}
	}
}

void FSkeletalAnimationPreview::PlayPreviewInDirection(int32 Direction)
{
	UAnimSingleNodeInstance* AnimSingleNodeInstance = GetPreviewAnimInstance();
	if (!AnimSingleNodeInstance || !HasPreviewClip())
	{
		return;
	}

	if (AnimSingleNodeInstance->IsPlaying() && PreviewPlayDirection == Direction)
	{
		AnimSingleNodeInstance->SetPlaying(false);
		return;
	}
	PreviewPlayDirection = Direction;
	ApplyPreviewPlayRate();
	if (!AnimSingleNodeInstance->IsLooping() && IsPreviewAtEnd())
	{
		if (PreviewPlayDirection == 1)
		{
			AnimSingleNodeInstance->SetPosition(0.0f);
		}
		else
		{
			AnimSingleNodeInstance->SetPosition(AnimSingleNodeInstance->GetLength());
		}
	}
	AnimSingleNodeInstance->SetPlaying(true);
}

void FSkeletalAnimationPreview::StepPreviewFrame(int32 Delta)
{
	UAnimSingleNodeInstance* AnimSingleNodeInstance = GetPreviewAnimInstance();
	if (!AnimSingleNodeInstance || !HasPreviewClip() || Delta == 0)
	{
		return;
	}

	const UAnimDataModel* Model = PreviewClips[SelectedClipIndex].Sequence->GetDataModel();
	const float ClipFPS = Model->GetFrameRate().AsDecimal();
	if (ClipFPS <= 0.0f)
	{
		return;
	}

	const float ExactFrame = AnimSingleNodeInstance->GetPosition() * ClipFPS;
	const int32 CurrentFrame = static_cast<int32>(std::floor(ExactFrame + 1e-3f));

	int32 TargetFrame = CurrentFrame + Delta;
	if (Delta < 0 && (ExactFrame - static_cast<float>(CurrentFrame)) > 1e-3f)
	{
		TargetFrame = CurrentFrame + Delta + 1;
	}

	TargetFrame = std::clamp(TargetFrame, 0, Model->GetNumberOfFrames());
	ScrubPreviewToTime(static_cast<float>(TargetFrame) / ClipFPS);
}

void FSkeletalAnimationPreview::CapturePreviewPlayback()
{
	auto* Instance = GetPreviewAnimInstance();
	if (!Instance || PreviewMode == EPreviewMode::None) return;
	auto& State = PreviewMode == EPreviewMode::Sequence ? SequencePlayback : BlendSpacePlayback;
	const float Length = Instance->GetLength();
	State.NormalizedTime = Length > 0 ? std::clamp(Instance->GetPosition() / Length, 0.0f, 1.0f) : 0.0f;
	State.Playing = Instance->IsPlaying();
	State.Looping = Instance->IsLooping();
	State.Rate = Instance->GetPlayRate();
}

void FSkeletalAnimationPreview::ResetPreviewModes()
{
 StopStateMachinePreview();
 StopAnimGraphPreview();
	if (auto* Instance = GetPreviewAnimInstance())
	{
		Instance->SetAnimationAsset(nullptr);
		Instance->SetPlaying(false);
	}
	PreviewMode = EPreviewMode::None;
	SelectedClipIndex = -1;
	SelectedBlendSpace = nullptr;
	SequencePlayback = FPreviewPlaybackState();
	BlendSpacePlayback = FPreviewPlaybackState();
	BlendSpaceInput = 0.0f;
	BlendSpaceMessage.clear();
	PreviewPlayDirection = 1;
	PreviewPlaySpeed = PreviewCustomSpeed = 1.0f;
}

bool FSkeletalAnimationPreview::HasActivePreview() const
{
	return (PreviewMode == EPreviewMode::StateMachine && GetStateMachinePreview()) || (PreviewMode == EPreviewMode::AnimGraph && GetAnimGraphPreview()) || HasPreviewClip() || (PreviewMode == EPreviewMode::BlendSpace && SelectedBlendSpace);
}

bool FSkeletalAnimationPreview::IsBlendSpacePlayable() const
{
	if (!SelectedBlendSpace || SelectedBlendSpace->GetSkeletalMesh() != Tab.SelectedMesh || SelectedBlendSpace->GetNumSamples() == 0) return false;
	const auto* Axis = SelectedBlendSpace->GetBlendParameter(0);
	if (!Axis || !std::isfinite(Axis->Min) || !std::isfinite(Axis->Max) || Axis->Min >= Axis->Max) return false;
	for (const auto& Sample : SelectedBlendSpace->GetSamples())
	{
		if (!Sample.Animation || Sample.Animation->GetSkeletalMesh() != Tab.SelectedMesh || !Sample.Animation->GetDataModel()) return false;
		const float Length = Sample.Animation->GetPlayLength();
		if (!std::isfinite(Length) || Length <= 0.0f || !std::isfinite(Sample.SampleValue.X) ||
			Sample.SampleValue.X < Axis->Min || Sample.SampleValue.X > Axis->Max) return false;
	}
	return true;
}

void FSkeletalAnimationPreview::RefreshPreviewPose()
{
	// Zero delta updates BlendSpace weights, evaluates the pose, and flushes skinning.
	if (Tab.PreviewComponent) Tab.PreviewComponent->TickComponent(0.0f);
}

void FSkeletalAnimationPreview::ActivatePreviewMode(EPreviewMode Mode)
{
 if (PreviewMode == EPreviewMode::StateMachine || PreviewMode == EPreviewMode::AnimGraph) { Tab.PreviewComponent->ClearAnimInstance(); Tab.PreviewComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode); Tab.PreviewComponent->InitializeAnimInstance(); PreviewMode=EPreviewMode::None; }
	auto* Instance = GetPreviewAnimInstance();
	if (!Instance) return;
	if (Mode == EPreviewMode::Sequence && (SelectedClipIndex < 0 || SelectedClipIndex >= PreviewClips.Num())) Mode = EPreviewMode::None;
	if (Mode == EPreviewMode::BlendSpace && !IsBlendSpacePlayable()) Mode = EPreviewMode::None;
	if (PreviewMode == Mode) return;
	CapturePreviewPlayback();
	PreviewMode = Mode;
	if (Mode == EPreviewMode::None)
	{
		Instance->SetAnimationAsset(nullptr);
		Instance->SetPlaying(false);
		Tab.PreviewComponent->ResetPoseToBindPose();
		Tab.PreviewComponent->FlushPendingPose();
		return;
	}
	const auto& State = Mode == EPreviewMode::Sequence ? SequencePlayback : BlendSpacePlayback;
	if (Mode == EPreviewMode::Sequence)
	{
		Instance->SetAnimationAsset(PreviewClips[SelectedClipIndex].Sequence);
		PreviewPlayDirection = State.Rate < 0 ? -1 : 1;
		PreviewPlaySpeed = std::abs(State.Rate);
		PreviewCustomSpeed = PreviewPlaySpeed;
	}
	else
	{
		Instance->SetBlendSpaceAsset(SelectedBlendSpace);
		const auto& Axis = *SelectedBlendSpace->GetBlendParameter(0);
		BlendSpaceInput = std::clamp(BlendSpaceInput, Axis.Min, Axis.Max);
		Instance->SetBlendSpaceInput(BlendSpaceInput);
	}
	Instance->SetLooping(State.Looping);
	Instance->SetPlayRate(State.Rate);
	Instance->SetPosition(State.NormalizedTime * Instance->GetLength());
	Instance->SetPlaying(State.Playing);
	RefreshPreviewPose();
}

void FSkeletalAnimationPreview::SelectPreviewBlendSpace(UBlendSpace1D* Asset)
{
	CapturePreviewPlayback();
	SelectedBlendSpace = Asset;
	BlendSpacePlayback = FPreviewPlaybackState();
	BlendSpaceMessage.clear();
	const auto* Axis = Asset ? Asset->GetBlendParameter(0) : nullptr;
	BlendSpaceInput = Axis ? Axis->Min : 0.0f;
	// Detach even on a same-mode selection so time and weights are reinitialized.
	if (auto* Instance = GetPreviewAnimInstance()) { Instance->SetAnimationAsset(nullptr); Instance->SetPlaying(false); }
	PreviewMode = EPreviewMode::None;
	if (Asset && !IsBlendSpacePlayable())
	{
		BlendSpaceMessage = "BlendSpace has no playable samples for this mesh. Use Edit Asset to configure it.";
		BlendSpacePlayback.Playing = false;
	}
	ActivatePreviewMode(EPreviewMode::BlendSpace);
	if (PreviewMode == EPreviewMode::None && Tab.PreviewComponent)
	{
		Tab.PreviewComponent->ResetPoseToBindPose();
		Tab.PreviewComponent->FlushPendingPose();
	}
}

void FSkeletalAnimationPreview::ApplyBlendSpaceInput(float Value)
{
	if (!std::isfinite(Value) || !IsBlendSpacePlayable()) return;
	ActivatePreviewMode(EPreviewMode::BlendSpace);
	const auto& Axis = *SelectedBlendSpace->GetBlendParameter(0);
	BlendSpaceInput = std::clamp(Value, Axis.Min, Axis.Max);
	if (auto* Instance = GetPreviewAnimInstance()) Instance->SetBlendSpaceInput(BlendSpaceInput);
	RefreshPreviewPose();
}

UAssetAnimInstance *FSkeletalAnimationPreview::GetStateMachinePreview() const
{
	return PreviewMode == EPreviewMode::StateMachine && Tab.PreviewComponent
			   ? Cast<UAssetAnimInstance>(Tab.PreviewComponent->GetAnimInstance())
			   : nullptr;
}

void FSkeletalAnimationPreview::StopStateMachinePreview()
{
	if (PreviewMode != EPreviewMode::StateMachine)
		return;
	PreviewMode = EPreviewMode::None;
	if (Tab.PreviewComponent)
	{
		Tab.PreviewComponent->ClearAnimInstance();
		Tab.PreviewComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Tab.PreviewComponent->InitializeAnimInstance();
	}
}

bool FSkeletalAnimationPreview::PreviewStateMachine(const FAnimStateMachineAssetData &Data, FString &Error)
{
	if (!Data.Validate(Error))
		return false;
	auto *mesh = UAssetManager::GetAssetByPath<USkeletalMesh>(Data.MeshKey);
	Tab.OpenMesh(mesh);
	if (!Tab.PreviewComponent)
	{
		Error = "Preview component is unavailable.";
		return false;
	}
	Tab.PreviewComponent->ClearAnimInstance();
	Tab.PreviewComponent->SetAnimationMode(EAnimationMode::StateMachine);
	Tab.PreviewComponent->InitializeAnimInstance();
	auto *instance = Cast<UAssetAnimInstance>(Tab.PreviewComponent->GetAnimInstance());
	if (!instance || !instance->Configure(Data, false))
	{
		Error = instance ? instance->GetLastError() : "Cannot create preview instance.";
		return false;
	}
	PreviewMode = EPreviewMode::StateMachine;
	Tab.bViewportOpen = true;
	Tab.RequestFocus();
	Tab.PreviewComponent->TickComponent(0);
	return true;
}

UAnimGraphInstance* FSkeletalAnimationPreview::GetAnimGraphPreview() const
{
    return PreviewMode == EPreviewMode::AnimGraph && Tab.PreviewComponent ? Cast<UAnimGraphInstance>(Tab.PreviewComponent->GetAnimInstance()) : nullptr;
}

void FSkeletalAnimationPreview::StopAnimGraphPreview()
{
    if (PreviewMode != EPreviewMode::AnimGraph) return;
    PreviewMode = EPreviewMode::None;
    if (Tab.PreviewComponent) { Tab.PreviewComponent->ClearAnimInstance(); Tab.PreviewComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode); Tab.PreviewComponent->InitializeAnimInstance(); }
}

bool FSkeletalAnimationPreview::PreviewAnimGraph(const FAnimGraphAssetData& Data, FString& Error)
{
    if (!Data.Validate(Error)) return false;
    Tab.OpenMesh(UAssetManager::GetAssetByPath<USkeletalMesh>(Data.MeshKey));
    if (!Tab.PreviewComponent) { Error = "Preview component is unavailable"; return false; }
    Tab.PreviewComponent->ClearAnimInstance();
    Tab.PreviewComponent->SetAnimationMode(EAnimationMode::AnimGraph);
    Tab.PreviewComponent->InitializeAnimInstance();
    auto* Instance = Cast<UAnimGraphInstance>(Tab.PreviewComponent->GetAnimInstance());
    if (!Instance || !Instance->Configure(Data,false)) { Error = Instance ? Instance->GetLastError() : "Cannot create AnimGraph preview"; return false; }
    PreviewMode = EPreviewMode::AnimGraph; Tab.bViewportOpen = true; Tab.RequestFocus(); Tab.PreviewComponent->TickComponent(0); return true;
}


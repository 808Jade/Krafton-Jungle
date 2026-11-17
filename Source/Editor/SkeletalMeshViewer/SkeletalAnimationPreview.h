#pragma once
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimNotifyEvent.h"
#include "Core/EngineString.h"
#include "Containers/Array.h"
#include <functional>

class FSkeletalPreviewTabBase;
class UAnimSequence;
class UAnimSequenceBase;
class UAnimSingleNodeInstance;
class UBlendSpace1D;
class UAssetAnimInstance;
class UAnimGraphInstance;
struct FAnimStateMachineAssetData;
struct FAnimGraphAssetData;

// 프리뷰 메시의 애니메이션 재생. 클립(시퀀스)·BlendSpace·State Machine·AnimGraph 미리보기 모드를 전환하고,
// 재생·일시정지·스크럽·속도·단축키, 노티파이 발동 표시를 맡는다. 노티파이 "편집"은 FSkeletalNotifyEditor가 한다.
// State Machine·AnimGraph 에디터는 탭의 이 객체(Animation)를 통해 미리보기를 건다.
class FSkeletalAnimationPreview
{
public:
	explicit FSkeletalAnimationPreview(FSkeletalPreviewTabBase& InTab) : Tab(InTab) {}

	struct FPreviewClip
	{
		FString Name;
		UAnimSequence* Sequence = nullptr;
	};
	enum class EPreviewMode { None, Sequence, BlendSpace, StateMachine, AnimGraph };
	struct FPreviewPlaybackState
	{
		float NormalizedTime = 0.0f;
		float Rate = 1.0f;
		bool Playing = true;
		bool Looping = true;
	};
	struct FFiredPreviewNotify
	{
		FAnimNotifyEvent Event;
		FString Label;
		double FiredAt = 0.0;
	};

	// --- 클립 ---
	UAnimSingleNodeInstance* GetPreviewAnimInstance() const;
	void ClearPreviewClips();
	void BuildPreviewClips();
	void SelectPreviewClip(int32 ClipIndex);
	UAnimSequence* GetSelectedPreviewSequence() const;
	bool HasPreviewClip() const;

	// --- 모드 전환 ---
	void CapturePreviewPlayback();
	void ActivatePreviewMode(EPreviewMode Mode);
	void ResetPreviewModes();
	bool HasActivePreview() const;
	// 미리보기 재생 중에는 본 편집·저장을 막는다. 막았으면 true다.
	bool BlockEditDuringAnimationPreview();

	// --- BlendSpace ---
	bool IsBlendSpacePlayable() const;
	void SelectPreviewBlendSpace(UBlendSpace1D* Asset);
	void ApplyBlendSpaceInput(float Value);
	void SetOpenBlendSpaceEditorCallback(std::function<void(const FString&)> Callback) { OnOpenBlendSpaceEditor = std::move(Callback); }

	// --- State Machine·AnimGraph (다른 에디터가 호출) ---
	bool PreviewAnimGraph(const FAnimGraphAssetData& Data, FString& Error);
	UAnimGraphInstance* GetAnimGraphPreview() const;
	void StopAnimGraphPreview();
	bool PreviewStateMachine(const FAnimStateMachineAssetData& Data, FString& Error);
	UAssetAnimInstance* GetStateMachinePreview() const;
	void StopStateMachinePreview();

	// --- 재생 제어 ---
	void RefreshPreviewPose();
	bool IsPreviewAtEnd() const;
	void UpdatePreviewPlaybackEnd();
	void TogglePreviewPlayback();
	int32 GetPreviewCurrentFrame() const;
	void ScrubPreviewToTime(float Seconds);
	void ApplyPreviewPlayRate();
	void PlayPreviewInDirection(int32 Direction);
	void StepPreviewFrame(int32 Delta);
	void SetPreviewPlaySpeed(float Speed);
	void HandlePreviewShortcuts();

	// --- 노티파이 발동 (프리뷰 컴포넌트의 델리게이트가 부른다) ---
	void HandlePreviewNotify(const FAnimNotifyEvent& Event, UAnimSequenceBase* Sequence);
	void HandlePreviewNotifyStateBegin(const FAnimNotifyEvent& Event, UAnimSequenceBase* Sequence);
	void HandlePreviewNotifyStateEnd(const FAnimNotifyEvent& Event, UAnimSequenceBase* Sequence);
	void AddFiredPreviewNotify(const FAnimNotifyEvent& Event, const FString& Label);
	bool IsPreviewNotifyFlashing(const FAnimNotifyEvent& Event) const;

	EPreviewMode PreviewMode = EPreviewMode::None;
	FPreviewPlaybackState SequencePlayback, BlendSpacePlayback;
	UBlendSpace1D* SelectedBlendSpace = nullptr; // Registered asset, non-owning.
	float BlendSpaceInput = 0.0f;
	FString BlendSpaceMessage;
	std::function<void(const FString&)> OnOpenBlendSpaceEditor;

	uint32 PreviewNotifyHandle = 0;
	uint32 PreviewNotifyStateBeginHandle = 0;
	uint32 PreviewNotifyStateEndHandle = 0;
	TArray<FFiredPreviewNotify> FiredNotifies;
	bool bPreviewCameraShake = true;

	TArray<FPreviewClip> PreviewClips;
	int32 SelectedClipIndex = -1; // -1 = None
	int32 PreviewPlayDirection = 1;
	float PreviewPlaySpeed = 1.0f;
	float PreviewCustomSpeed = 1.0f;

private:
	// 참조(집약): 이 객체를 가진 탭. 프리뷰 컴포넌트·메시·다른 편집 상태를 읽는다. 탭이 이 객체보다 오래 산다.
	FSkeletalPreviewTabBase& Tab;
};

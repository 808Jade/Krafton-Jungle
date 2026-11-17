#pragma once
#include "Core/EngineString.h"
#include "Containers/Array.h"

class FSkeletalPreviewTabBase;
class UClass;
struct FProperty;

// 선택한 애니메이션 시퀀스의 노티파이 편집. 추가·삭제·이동·길이·이름·클래스 변경과 .notify 파일 저장을 맡는다.
// 편집 UI는 Animation 패널이 그린다. 클립이 바뀌면 애니메이션 미리보기가 선택을 지우므로 탭이 가진다.
class FSkeletalNotifyEditor
{
public:
	explicit FSkeletalNotifyEditor(FSkeletalPreviewTabBase& InTab) : Tab(InTab) {}

	void SelectPreviewNotify(int32 Index);
	TArray<FString> CollectUsedNotifyNames() const;
	void RefreshAudioFileList(const char* Folder);

	int32 AddPreviewNotify(float Time, float Duration);
	void RemovePreviewNotify(int32 Index);
	void SetPreviewNotifyTime(int32 Index, float Time, bool bSnapToFrame);
	int32 SortPreviewNotifies(int32 KeepIndex);
	void SetPreviewNotifyDuration(int32 Index, float Duration);
	void RenamePreviewNotify(int32 Index, const FString& Name);
	void SetPreviewNotifyClass(int32 Index, UClass* NotifyClass);
	void OnPreviewNotifyPropertyChanged(int32 Index, const FProperty& Property);
	bool SavePreviewNotifies();

	TArray<FString> AudioFileList;
	int32 SelectedNotifyIndex = -1;
	bool bDraggingNotify = false;
	float NotifyDragOffset = 0.0f;
	int32 NotifyPopupIndex = -1;
	float NotifyPopupTime = 0.0f;
	char NotifyNameBuffer[64] = "";
	bool bNotifyDirty = false;

private:
	// 참조(집약): 이 객체를 가진 탭. 선택한 시퀀스는 탭의 애니메이션 미리보기에서 얻는다. 탭이 이 객체보다 오래 산다.
	FSkeletalPreviewTabBase& Tab;
};

#pragma once
#include "Editor/SkeletalMeshViewer/SkeletalPreviewTabBase.h"
#include "Animation/AnimGraph.h"
#include "Animation/AssetAnimInstance.h"

// 메시 뷰어와 같은 프리뷰 탭 구현(FSkeletalPreviewTabBase)을 화면에 띄우지 않은 채 쓴다. 프리뷰 월드·타깃·재생은 문서마다 따로다.
// Compile은 설정을 끝낸 새 백엔드로 바꿔 끼운다. 그리기는 엔진이 CollectViews로 받은 View로 한다.
class FAnimationPreviewSession
{
public:
    bool Compile(FSkeletalPreviewTabBase& Services, const FAnimGraphAssetData& Data, FString& Error);
    bool Compile(FSkeletalPreviewTabBase& Services, const FAnimStateMachineAssetData& Data, FString& Error);
    void Tick(float DeltaSeconds) { if(Backend) Backend->Tick(DeltaSeconds); }
    void CollectViews(TArray<FEditorTabView>& OutViews) { if(Backend) Backend->CollectViews(OutViews); }
    void Draw() { if(Backend) Backend->DrawEmbeddedPreview(); else ImGui::TextDisabled("Compile to preview this animation."); }
    void Hide() { if(Backend) Backend->HideEmbeddedPreview(); }
    FSkeletalPreviewTabBase* GetBackend() const { return Backend.get(); }
    UAnimInstance* GetInstance() const;
    void SetPaused(bool Paused);
    bool IsPaused() const;
private:
    std::unique_ptr<FSkeletalPreviewTabBase> Backend;
};

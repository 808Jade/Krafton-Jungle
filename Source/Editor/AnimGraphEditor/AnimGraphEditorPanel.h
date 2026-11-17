#pragma once
#include "Editor/EditorUI/EditorPanel.h"
#include "AnimPoseGraphEditor.h"
#include <memory>
#include <vector>
class FSkeletalPreviewTabBase;
struct FEditorTabView;
class FStateMachineEditorPanel;
class FAnimGraphEditorPanel : public IEditorPanel
{
public:
    FAnimGraphEditorPanel();
    ~FAnimGraphEditorPanel() override;
    bool Init() override;
    void Tick(float DeltaSeconds) override;
    // 보이는 문서의 미리보기 View를 엔진에 넘긴다.
    void CollectPreviewViews(TArray<FEditorTabView>& OutViews);
    void OnRender() override;
    const char* GetPanelName() const override { return "AnimGraph Editor"; }
    void SetOpen(bool Open) override;
    void OpenAsset(const FString& Path);
    void NewAsset(const FString& Folder);
    void NewStateMachine(const FString& Folder);
    void SetViewer(FSkeletalPreviewTabBase* V) { Viewer=V; }
    void SetStateMachineTemplate(FStateMachineEditorPanel* V) { StateTemplate=V; }
    bool IsKeyboardFocused() const { return IsOpen() && Focused; }
    void ReleaseResources();
private:
    struct FDocument;
    struct FSnapshot;
    struct FLocation { int Document=0, Pose=-1, Rule=0; bool operator==(const FLocation&) const = default; };
    FDocument* Current();
    FDocument* FindDocument(int Id);
    void OpenPending();
    void OpenReference(const FString& Path);
    void SaveUISettings();
    void Activate(int Id, bool Record=true);
    bool Save(FDocument& Doc);
    bool Compile(FDocument& Doc);
    void DrawToolbar(FDocument& Doc);
    void DrawCanvas(FDocument& Doc);
    void DrawTree(FDocument& Doc);
    void DrawDetails(FDocument& Doc);
    void DrawAssets(FDocument& Doc);
    void DrawResults(FDocument& Doc);
    void DrawPreviewParameters(FDocument& Doc);
    void CollectDiagnostics(FDocument& Doc);
    void CommitEdits(FDocument& Doc);
    void Undo(FDocument& Doc, bool Redo);
    FSnapshot Capture(FDocument& Doc);
    void Restore(FDocument& Doc,const FSnapshot& Snapshot);
    void RecordLocation();
    void Navigate(int Direction);
    void ShowLocation(const FLocation& Location);
    void SetupDockspace(ImGuiID DockId, ImVec2 Size);
    std::vector<std::unique_ptr<FDocument>> Documents;
    std::vector<FLocation> Locations;
    int PendingParent=0;
    bool PaneOpen[6]={true,true,true,true,true,true};
    int LocationIndex=-1, Active=0, NextDocument=1, SelectTab=0, CloseDocument=0;
    FSkeletalPreviewTabBase* Viewer=nullptr;
    FStateMachineEditorPanel* StateTemplate=nullptr;
    FString PendingPath, PendingFolder, Error;
    bool NewMachine=false, Focused=false, Visible=false, ResetLayout=false, CloseAll=false;
};
void ReloadAnimationAssetInstances(const FString& Path);

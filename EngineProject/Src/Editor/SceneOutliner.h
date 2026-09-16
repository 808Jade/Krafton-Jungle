#pragma once
#include <functional>

class UWorld;
class ULevel;
class AActor;
class UActorComponent;
class UPrimitiveComponent;

using SelectionCallback = std::function<void(UPrimitiveComponent*)>;

class FSceneOutliner : public IEditorPanel
{
public:
    FSceneOutliner() = default;
    ~FSceneOutliner() = default;

    void SetSelectionCallback(SelectionCallback InCallback) { Callback = InCallback; }

public:
    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;

    void SetWorld(UWorld* InWorld) { World = InWorld; };

    void DrawActors(ULevel* Level);
    void DrawActorNode(AActor* Actor);
    void SelectActor(AActor* Actor);


private:
    UWorld* World = nullptr;
    UObject* SelectedObject = nullptr;

    SelectionCallback Callback;
};
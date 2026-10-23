#pragma once
#include <functional>

class UWorld;
class ULevel;
class AActor;
class UActorComponent;
class UPrimitiveComponent;

using SelectionCallback = std::function<void(UPrimitiveComponent*)>;
using DeleteActorCallback = std::function<void(AActor*)>;

class FSceneOutliner : public IEditorPanel
{
public:
    FSceneOutliner() = default;
    ~FSceneOutliner() = default;

    void SetSelectionCallback(SelectionCallback InCallback) { Callback = InCallback; }
    void SetDeleteActorCallback(DeleteActorCallback InCallback) { DeleteCallback = InCallback;}

public:
    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;

    void SetWorld(UWorld* InWorld) { World = InWorld; };

    void DrawActors(ULevel* Level);
    void DrawActorNode(AActor* Actor);
    void SelectActor(AActor* Actor);

    AActor* GetSelectedActor() const;


private:
    UWorld* World = nullptr;
    UObject* SelectedObject = nullptr;

    SelectionCallback Callback;
    DeleteActorCallback DeleteCallback;
    AActor* PendingDeleteActor = nullptr;
};
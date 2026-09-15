#pragma once

class UWorld;
class ULevel;
class AActor;
class UActorComponent;

class FSceneOutliner : public IEditorPanel
{
public:
    FSceneOutliner() = default;
    ~FSceneOutliner() = default;

public:
    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;

    void SetWorld(UWorld* InWorld) { World = InWorld; };

    void DrawActors(ULevel* Level);
    void DrawActorNode(AActor* Actor);
    void DrawComponents(AActor* Actor);
    void DrawComponentNode(UActorComponent* Component);
    void SelectObject(UObject* Object);


private:
    UWorld* World = nullptr;
    UObject* SelectedObject = nullptr;
};
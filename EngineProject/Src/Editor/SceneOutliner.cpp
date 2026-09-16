#include "EnginePCH.h"
#include "SceneOutliner.h"

#include "Engine/Level.h"
#include "Engine/World.h"

#include "Component/ActorComponent.h"
#include "Component/PrimitiveComponent.h"

bool FSceneOutliner::Init()
{
    return true;
}

void FSceneOutliner::Tick(float DeltaTime)
{
}

void FSceneOutliner::OnRender()
{
    // 1. World 참조 확인
    if (!World)
        return;

    // 2. 현재 Level 가져오기
    ULevel* Level = World->GetCurrentLevel();
    if (!Level)
        return;

    // 3. Level의 Actor 목록 Tree 출력
    ImGui::Begin("Scene Manager");
    DrawActors(Level);

    ImGui::End();
}

// Level이 가지고 있는 Actor 목록 순회하며 Draw TreeNode
void FSceneOutliner::DrawActors(ULevel* Level)
{
    if (!Level)
        return;

    const TArray<AActor*>& Actors = Level->GetActors();

    struct FActorGroup
    {
        EPrimitiveType Type;
        const char* Name;
    };

    const FActorGroup Groups[] =
    {
        { EPrimitiveType::Sphere, "Sphere" },
        { EPrimitiveType::Cube,   "Cube" },
        { EPrimitiveType::Cone,   "Cone" },
        { EPrimitiveType::Plane,  "Plane" }
    };

    for (AActor* Actor : Actors)
    {
        if (!Actor)
            continue;

        DrawActorNode(Actor);
    }
}

// Actor를 ImGui TreeNode로 출력
void FSceneOutliner::DrawActorNode(AActor* Actor)
{
    if (!Actor)
        return;

    const FString& ActorName = Actor->GetName();

    //ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_None;
    ImGuiTreeNodeFlags Flags = 
        ImGuiTreeNodeFlags_Leaf |
        ImGuiTreeNodeFlags_NoTreePushOnOpen |
        ImGuiTreeNodeFlags_SpanAvailWidth;

    // Actor 선택 시 UI 선택 상태
    if (SelectedObject == Actor)
        Flags |= ImGuiTreeNodeFlags_Selected;
    bool bOpened = ImGui::TreeNodeEx(Actor, Flags, "%s", ActorName.c_str());

    // UI에서 Actor 클릭
    if (ImGui::IsItemClicked())
    {
        SelectActor(Actor);
    }
    
}

// 선택 Object 변경
void FSceneOutliner::SelectActor(AActor* Actor)
{
    if (!Actor)
        return;

    SelectedObject = Actor;

    LOG(Info, "{} UUID {} is selected", SelectedObject->GetName(), SelectedObject->GetUUID());

    UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent());

    if (Callback)
        Callback(Primitive);
}
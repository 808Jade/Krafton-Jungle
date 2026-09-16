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

    //for (AActor* Actor : Actors)
    //{
    //    if (!Actor)
    //        continue;

    //    DrawActorNode(Actor);
    //}

    for (const FActorGroup& Group : Groups)
    {
        bool bHasActor = false;

        for (AActor* Actor : Actors)
        {
            if (!Actor)
                continue;

            UPrimitiveComponent* Primitive =
                Cast<UPrimitiveComponent>(Actor->GetRootComponent());

            if (Primitive && Primitive->GetType() == Group.Type)
            {
                bHasActor = true;
                break;
            }
        }

        if (!bHasActor)
            continue;

        if (ImGui::TreeNodeEx(Group.Name, ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (AActor* Actor : Actors)
            {
                if (!Actor)
                    continue;

                UPrimitiveComponent* Primitive =
                    Cast<UPrimitiveComponent>(Actor->GetRootComponent());

                if (Primitive && Primitive->GetType() == Group.Type)
                {
                    DrawActorNode(Actor);
                }
            }

            ImGui::TreePop();
        }
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
        ImGuiTreeNodeFlags_NoTreePushOnOpen;

    // Actor 선택 시 UI 선택 상태
    if (SelectedObject == Actor) 
    { 
        Flags |= ImGuiTreeNodeFlags_Selected; 
    } 

    bool bOpened = ImGui::TreeNodeEx(Actor, Flags, "%s", ActorName.c_str());

    // UI에서 Actor 클릭
    if (ImGui::IsItemClicked())
    {
        SelectObject(Actor);
    }

    //// Component Draw
    //if (bOpened) 
    //{
    //    DrawComponents(Actor);
    //    ImGui::TreePop();
    //}
    
}

// Actor가 가지고 있는 Component 목록 순회
void FSceneOutliner::DrawComponents(AActor* Actor)
{
    if (!Actor)
        return;

    const TArray<UActorComponent*>& Components = Actor->GetComponents();

    for (UActorComponent* Component : Components)
    {
        if (!Component)
            continue;

        DrawComponentNode(Component);
    }
    
}

// Component를 Actor의 자식 노드로 출력
void FSceneOutliner::DrawComponentNode(UActorComponent* Component)
{
    if (!Component)
        return;

    FString ComponentName = Component->GetName();

    ImGuiTreeNodeFlags Flags =
        ImGuiTreeNodeFlags_Leaf |                // Leaf Node (추후 Component 추가 시 수정필요)
        ImGuiTreeNodeFlags_NoTreePushOnOpen;     // TreePush()를 추가로 하지 않음

    // Component 선택 시 UI 선택 상태
    if (SelectedObject == Component)  
    { 
        Flags |= ImGuiTreeNodeFlags_Selected; 
    }  

    ImGui::TreeNodeEx(Component, Flags, "%s", ComponentName.c_str());

    if (ImGui::IsItemClicked())
    {
        SelectObject(Component);
    }
}

// 선택 Object 변경
void FSceneOutliner::SelectObject(UObject* Object)
{
    SelectedObject = Object;

    AActor* Actor = Cast<AActor>(Object);
    if (!Actor)
        return;

    UPrimitiveComponent* Primitive =
        Cast<UPrimitiveComponent>(Actor->GetRootComponent());

    if (Callback)
    {
        Callback(Primitive);
    }
}
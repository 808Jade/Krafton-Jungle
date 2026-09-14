#include "EnginePCH.h"
#include "Component/SceneComponent.h"

FMatrix USceneComponent::GetWorldMatrix() const
{
    //parent를 추가해야할 경우
    return transform.GetWorldMatrix();
}

FTransform* USceneComponent::GetTransform()
{
    return &transform;
}

void USceneComponent::SetTransform(FTransform Transform)
{
    transform = Transform;
}

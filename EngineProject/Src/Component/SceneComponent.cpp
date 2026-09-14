#include "EnginePCH.h"
#include "Component/SceneComponent.h"

FRotator USceneComponent::GetWorldRotation() const
{
	if (AttachParent)
	{
		FQuat ParentQuat = AttachParent->GetWorldRotation().Quaternion();
		FQuat LocalQuat = RelativeTransform.GetOrientation();

		return (ParentQuat * LocalQuat).ToFRotator();
	}

	return RelativeTransform.Rotation;
}

FVector USceneComponent::GetWorldLocation() const
{
	FMatrix WorldMatrix = GetWorldMatrix();

	return FVector(WorldMatrix[3][0], WorldMatrix[3][1], WorldMatrix[3][2]);
}

FVector USceneComponent::GetWorldScale3D() const
{
	if (AttachParent)
	{
		FVector ParentScale = AttachParent->GetWorldScale3D();

		return FVector(
			RelativeTransform.Scale.X * ParentScale.X,
			RelativeTransform.Scale.Y * ParentScale.Y,
			RelativeTransform.Scale.Z * ParentScale.Z
		);
	}

	return RelativeTransform.Scale;
}

FMatrix USceneComponent::GetWorldMatrix() const
{
	FMatrix LocalMatrix = RelativeTransform.GetWorldMatrix(); // 부모 컴포넌트 연결 없을 때

	if (AttachParent)
	{
		return LocalMatrix * AttachParent->GetWorldMatrix();
	}

	return LocalMatrix;
}


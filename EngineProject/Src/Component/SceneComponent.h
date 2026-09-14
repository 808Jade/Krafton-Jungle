#pragma once

#include "../Math/Transform.h"
#include "ActorComponent.h"

class USceneComponent : public UActorComponent
{
	DECLARE_CLASS(USceneComponent, UActorComponent)

	REFLECT_START(ClassName)
		PROPERTY(bVisible)
	REFLECT_END()

public:
	USceneComponent() = default;
	virtual ~USceneComponent() override = default;

	// Get & Set
	const FVector& GetRelativeLocation() const { return RelativeTransform.Location; }
	void SetRelativeLocation(const FVector& InLocation) { RelativeTransform.Location = InLocation; }

	const FRotator& GetRelativeRotation() const { return RelativeTransform.Rotation; }
	void SetRelativeRotation(const FRotator& InRotation) { RelativeTransform.Rotation = InRotation; }

	const FVector& GetRelativeScale3D() const { return RelativeTransform.Scale; }
	void SetRelativeScale3D(const FVector& InScale) { RelativeTransform.Scale = InScale; }

	// 쿼터니언 적용된 회전행렬
	FQuat GetRelativeRotationQuat() const { return RelativeTransform.GetOrientation(); } 

	const FTransform& GetTransform() const { return RelativeTransform; }
	void SetTransform(const FTransform& InTransform) { RelativeTransform = InTransform; }

	// Attatch-To
	USceneComponent* GetAttachParent() const { return AttachParent; }
	void SetupAttachment(USceneComponent* InParent) { AttachParent = InParent; }

	bool IsVisible() const { return bVisible; }
	void SetVisible(bool bInVisible) { bVisible = bInVisible; }

	FVector GetWorldLocation() const;
	FRotator GetWorldRotation() const;
	FVector GetWorldScale3D() const;
	FMatrix GetWorldMatrix() const;

protected:
	FTransform RelativeTransform;

	USceneComponent* AttachParent = nullptr; // Attach 부모 정보

	bool bVisible = true;
};
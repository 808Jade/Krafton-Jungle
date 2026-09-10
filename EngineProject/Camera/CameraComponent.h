#pragma once

#include "../Component/SceneComponent.h"
#include "../Math/EngineMath.h"
#include "../Math/Rotator.h"


class UCameraComponent : public USceneComponent
{
	DECLARE_CLASS(UCameraComponent, USceneComponent)
public:
	UCameraComponent() = default;
//private:
//	FTransform transform; // <- USceneComponent 상속
protected:
	/*FVector Location = FVector(0, 0, 0);
	FVector Scale = FVector(1, 1, 1);
	FRotator Rotation = FRotator(0, 0, 0);*/
public:
	float FOV = 60.0f;
	float AspectRatio = 16.0f / 9.0f;
	float NearClipPlane = 0.1f;
	float FarClipPlane = 10000.0f;
	bool bIsOrthogonal = false;
	float OrthoWidth = 16.0f;

public:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	void SetLocation(FVector vector);
	void SetScale(FVector vector);
	//void SetRotation(FVector vector);
	void SetFOV(float FoV);
	void SetAspectRatio(float Ratio);
	void SetNearClipPlane(float NearPlane);
	void SetFarClipPlane(float FarPlane);

	float* GetFOV();

	FVector GetLocation() const;
	FVector GetScale() const;
	FRotator GetRotation() const;

	FRay DeProjection(int32 MouseX, int32 MouseY, uint32 ScreenW, uint32 ScreenH);

	FMatrix GetViewMatrix() const;
	FMatrix GetOrthogonalMatrix() const;
	FMatrix GetPerspectiveMatrix() const;
	FMatrix GetViewProjectionMatrix() const;

private:
	const float CameraSpeed = 10.0f;
	const float MouseSensitivity = 0.05f;
	const float WheelSpeed = 0.1f;
};


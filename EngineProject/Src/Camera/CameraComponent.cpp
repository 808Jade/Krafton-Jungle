#include "EnginePCH.h"
#include "CameraComponent.h"
#include "Input/InputSystem.h"
#include "../Collision/Ray.h"

void UCameraComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    UpdateMovement(DeltaTime);
    UpdateRotation(DeltaTime);
    UpdateZoom(DeltaTime);
}

// 마우스 조작

void UCameraComponent::UpdateMovement(float DeltaTime)
{
    if (FInputSystem::IsKeyDown(EKeyCode::W)) RelativeTransform.Location += RelativeTransform.GetForward() * MoveSpeed * DeltaTime;
    if (FInputSystem::IsKeyDown(EKeyCode::S)) RelativeTransform.Location -= RelativeTransform.GetForward() * MoveSpeed * DeltaTime;
    if (FInputSystem::IsKeyDown(EKeyCode::A)) RelativeTransform.Location -= RelativeTransform.GetRight() * MoveSpeed * DeltaTime;
    if (FInputSystem::IsKeyDown(EKeyCode::D)) RelativeTransform.Location += RelativeTransform.GetRight() * MoveSpeed * DeltaTime;
    if (FInputSystem::IsKeyDown(EKeyCode::Q)) RelativeTransform.Location -= RelativeTransform.GetUp() * MoveSpeed * DeltaTime;
    if (FInputSystem::IsKeyDown(EKeyCode::E)) RelativeTransform.Location += RelativeTransform.GetUp() * MoveSpeed * DeltaTime;
}

void UCameraComponent::UpdateRotation(float DeltaTime)
{
    if (!FInputSystem::IsMouseDown(EMouseButton::Right)) return;

    RelativeTransform.Rotation.Pitch += FInputSystem::GetMouseDeltaY() * MouseSensitivity;
    RelativeTransform.Rotation.Yaw += FInputSystem::GetMouseDeltaX() * MouseSensitivity;
}

void UCameraComponent::UpdateZoom(float DeltaTime)
{
    int32 WheelDelta = FInputSystem::GetWheelDelta();
    if (WheelDelta == 0) return;

    RelativeTransform.Location += RelativeTransform.GetForward() * WheelSpeed * WheelDelta * DeltaTime;
}


FRay UCameraComponent::DeProjection(FVector2 MousePos, float ScreenW, float ScreenH)
{
    // 마우스 화면 좌표를 NDC 좌표(-1 ~ 1)로 변환
    const float NDCX = 2.0f * MousePos.X / ScreenW - 1.0f;
    const float NDCY = 1.0f - 2.0f * MousePos.Y / ScreenH;


    // Projection / View 행렬의 역행렬
    const FMatrix InvProjection = GetProjectionMatrix().Inverse();
    const FMatrix InvView = GetViewMatrix().Inverse();


    // NDC 공간의 Near / Far 지점
    FVector4 NearPoint(NDCX, NDCY, 0.0f, 1.0f);
    FVector4 FarPoint(NDCX, NDCY, 1.0f, 1.0f);


    // Projection 역변환
    NearPoint = NearPoint * InvProjection;
    FarPoint = FarPoint * InvProjection;


    // Perspective Divide 복원
    NearPoint /= NearPoint.W;
    FarPoint /= FarPoint.W;


    // View 역변환
    NearPoint = NearPoint * InvView;
    FarPoint = FarPoint * InvView;


    // Homogeneous 좌표 정리
    NearPoint /= NearPoint.W;
    FarPoint /= FarPoint.W;

    FRay Ray;

    if (bIsOrthogonal)
    {
        Ray.Origin = FVector(NearPoint.X, NearPoint.Y, NearPoint.Z);
    }
    // 원근 투영은 모든 Ray가 카메라 위치에서 시작
    else
    {
        Ray.Origin = GetWorldLocation();
    }

    // 시작점에서 Far 지점을 향하는 방향을 Ray 방향으로 사용
    Ray.Direction = (FVector(FarPoint.X, FarPoint.Y, FarPoint.Z) - Ray.Origin).Normalized();

    return Ray;
}

FMatrix UCameraComponent::GetViewMatrix() const
{
    //return GetWorldMatrix().Inverse();

    FVector WorldLocation = GetWorldLocation();
    FRotator WorldRotation = GetWorldRotation();

    FQuat WorldQuat = WorldRotation.Quaternion();
    FMatrix RotationMatrix = WorldQuat.ToFMatrix();

    FMatrix InverseTranslation = FMatrix::MakeTranslation(WorldLocation * -1.0f);
    FMatrix InverseRotation = RotationMatrix.GetTransposed();

    return InverseTranslation * InverseRotation;
}

FMatrix UCameraComponent::GetPerspectiveMatrix() const
{
	const float HalfFOV = FMath::DegreesToRadians(FieldOfView) * 0.5f;

	const float YScale = 1.0f / tan(HalfFOV);
	const float XScale = YScale / AspectRatio;

    const float ZScale = FarClipPlane / (FarClipPlane - NearClipPlane);
    const float ZOffset = -NearClipPlane * FarClipPlane /(FarClipPlane - NearClipPlane);

    //Forward가 X인 좌표
	return FMatrix(
		0.0f, 0.0f, ZScale, 1.0f,
		XScale, 0.0f, 0.0f, 0.0f,
		0.0f, YScale, 0.0f, 0.0f,
		0.0f, 0.0f, ZOffset, 0.0f
	);
}

FMatrix UCameraComponent::GetOrthogonalMatrix() const
{
    const float Width = OrthoWidth;
    const float Height = Width / AspectRatio;

    const float XScale = 2.0f / Width;
    const float YScale = 2.0f / Height;

    const float ZScale = 1.0f / (FarClipPlane - NearClipPlane);
    const float ZOffset = -NearClipPlane / (FarClipPlane - NearClipPlane);

    return FMatrix(
        0.0f, 0.0f, ZScale, 0.0f,
        XScale, 0.0f, 0.0f, 0.0f,
        0.0f, YScale, 0.0f, 0.0f,
        0.0f, 0.0f, ZOffset, 1.0f
    );
}

FMatrix UCameraComponent::GetProjectionMatrix() const
{
    if (bIsOrthogonal)
        return GetOrthogonalMatrix();  // 직교 투영
     
    return GetPerspectiveMatrix();     // 원근 투영
}

FMatrix UCameraComponent::GetViewProjectionMatrix() const
{
    return GetViewMatrix() * GetProjectionMatrix();
}

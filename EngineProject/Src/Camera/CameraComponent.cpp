#include "EnginePCH.h"
#include "CameraComponent.h"
#include "Input/InputSystem.h"
#include "../Collision/Ray.h"

//UCameraComponent::UCameraComponent()
//{
//
//}

void UCameraComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UCameraComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    if (FInputSystem::IsKeyDown(EKeyCode::W))
    {
        RelativeTransform.Location += RelativeTransform.GetForward() * MoveSpeed * DeltaTime;
    }

    if (FInputSystem::IsKeyDown(EKeyCode::S))
    {
        RelativeTransform.Location -= RelativeTransform.GetForward() * MoveSpeed * DeltaTime;
    }

    if (FInputSystem::IsKeyDown(EKeyCode::A))
    {
        RelativeTransform.Location -= RelativeTransform.GetRight() * MoveSpeed * DeltaTime;
    }

    if (FInputSystem::IsKeyDown(EKeyCode::D))
    {
        RelativeTransform.Location += RelativeTransform.GetRight() * MoveSpeed * DeltaTime;
    }

    if (FInputSystem::IsKeyDown(EKeyCode::Q))
    {
        RelativeTransform.Location -= RelativeTransform.GetUp() * MoveSpeed * DeltaTime;
    }

    if (FInputSystem::IsKeyDown(EKeyCode::E))
    {
        RelativeTransform.Location += RelativeTransform.GetUp() * MoveSpeed * DeltaTime;
    }

    // 우클릭 카메라 회전
    if (FInputSystem::IsMouseDown(EMouseButton::Right))
    {
        float DeltaPitch = FInputSystem::GetMouseDeltaY() * MouseSensitivity;
        float DeltaYaw = FInputSystem::GetMouseDeltaX() * MouseSensitivity;

        RelativeTransform.Rotation.Pitch += DeltaPitch;
        RelativeTransform.Rotation.Yaw += DeltaYaw;
    }

    // 마우스 휠 전진 / 후진
    int32 WheelDelta = FInputSystem::GetWheelDelta();

    if (WheelDelta != 0)
    {
        RelativeTransform.Location +=
            RelativeTransform.GetForward() * WheelSpeed * WheelDelta * DeltaTime;
    }
}

FRay UCameraComponent::DeProjection(int32 MouseX, int32 MouseY, uint32 ScreenW, uint32 ScreenH)
{
    // 마우스 화면 좌표를 NDC 좌표(-1 ~ 1)로 변환
    const float NDCX = 2.0f * MouseX / ScreenW - 1.0f;
    const float NDCY = 1.0f - 2.0f * MouseY / ScreenH;


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


    //// Near Plane 위의 점
    //FVector4 NDCPoint(NDCX, NDCY, 1.0f, 1.0f);
    //FVector4 CameraPoint = NDCPoint * InvPerspective;
    //FVector4 WorldPoint = CameraPoint * InvView;

    //CameraPoint /= CameraPoint.W;
    //WorldPoint /= WorldPoint.W;

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

    //ray.Origin = transform.Location;
    //ray.Direction = (FVector(WorldPoint.X, WorldPoint.Y, WorldPoint.Z) - ray.Origin).Normalized();

    //if (bIsOrthogonal) 
    //{
    //    const FMatrix InvOrthogonal = GetOrthogonalMatrix().Inverse();

    //    FVector4 NearPoint = FVector4(NDCX, NDCY, 0.0f, 1.0f) * InvOrthogonal;
    //    FVector4 FarPoint = FVector4(NDCX, NDCY, 1.0f, 1.0f) * InvOrthogonal;

    //    NearPoint /= NearPoint.W;
    //    FarPoint /= FarPoint.W;

    //    NearPoint = NearPoint * InvView;
    //    FarPoint = FarPoint * InvView;

    //    NearPoint /= NearPoint.W;
    //    FarPoint /= FarPoint.W;

    //    FRay ray;

    //    ray.Origin = FVector(NearPoint.X, NearPoint.Y, NearPoint.Z);
    //    FVector4 Direction4 = (FarPoint - NearPoint);

    //    ray.Direction = FVector(Direction4.X, Direction4.Y, Direction4.Z).Normalized();

    //    return ray;
    //}
    //
    //
    //return ray;
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

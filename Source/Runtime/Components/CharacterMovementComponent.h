#pragma once

#include "Components/MovementComponent.h"

struct FHitResult;

class UCharacterMovementComponent : public UMovementComponent
{
	DECLARE_CLASS(UCharacterMovementComponent, UMovementComponent)

public:
	UCharacterMovementComponent();

	void BeginPlay() override;
	void TickComponent(float DeltaTime) override;

	// 월드 좌표 기준 방향.
	// 값은 다음 SetMoveInput 또는 Stop까지 유지된다.
	void SetMoveInput(const FVector& WorldDirection);
	void SetMaxSpeed(float InMaxSpeed);

	void SetAcceleration(float InAcceleration);
	void SetBrakingDeceleration(float InDeceleration);

	void SetVerticalVelocity(float Value);
	float GetVerticalVelocity() const;

	void SetGravityAcceleration(float InGravity);
	void SetGravityScale(float InScale);

	FVector GetVelocity() const { return Velocity; }
	float GetGroundSpeed() const;

	bool IsGrounded() const { return bIsGrounded; }

	// 켜면 bBlockComponent가 켜진 스태틱 메시의 실제 표면을 바닥으로 걷는다. (울퉁불퉁한 지형 메시용, 기본은 꺼짐)
	// 벽은 지금처럼 Shape(Box 등)로 막는다. 꺼져 있으면 예전처럼 Shape만 바닥으로 본다.
	void SetWalkOnBlockingMeshes(bool bEnable) { bWalkOnBlockingMeshes = bEnable; }
	bool IsWalkingOnBlockingMeshes() const { return bWalkOnBlockingMeshes; }

	void StopMovementImmediately() override;

private:
	bool FindFloor(FHitResult& OutHit) const;
	void UpdateGroundedState();

	// 바닥 메시(bBlockComponent가 켜진 스태틱 메시)의 실제 표면을 찾는다. 캡슐 바닥 위 UpReach에서 아래 DownReach까지
	// 작은 구를 쓸어 내려서, 처음 닿은 면에 대해 OutGap에 "캡슐 바닥이 그 면보다 얼마나 위에 있는가"를 준다.
	// (음수면 면이 캡슐 바닥보다 위에 있다) 닿은 면이 없으면 false, 닿았지만 너무 가파르면 bOutWalkable = false.
	bool FindMeshFloor(float UpReach, float DownReach, float& OutGap, bool& bOutWalkable) const;
	// 걷는 중에 바닥 메시를 따라 캡슐 높이를 맞춘다. 한 걸음보다 높은 곳이면 수평 이동을 되돌린다.
	void FollowMeshFloor(const FVector& LocationBeforeMove);
	// 떨어지는 중에 바닥 메시 아래로 내려갔으면 표면으로 올려서 착지시킨다.
	bool LandOnMeshFloor(float FallDistance);

private:
	FVector MoveInput = FVector::ZeroVector;

	float MaxSpeed = 5.0f;
	float Acceleration = 20.0f;
	float BrakingDeceleration = 30.0f;

	// Gravity는 부모 클래스의 기존 멤버를 사용한다.
	float GravityScale = 1.0f;
	float MaxFallSpeed = 30.0f;


	bool bIsGrounded = false;

	// 캐릭터 충돌체 전체를 이 거리만큼 아래로 Sweep하여 지면 충돌 여부를 판단한다.
	float GroundProbeDistance = 0.05f;
	// 바닥 탐색 거리와 실제 접지 허용 거리를 구분한다.
	float GroundContactDistance = 0.003f;

	// 걸을 수 있는 최대 경사 약 45도.
	// cos(45) = 0.7071
	float WalkableFloorZ = 0.70710678;

	// 바닥에 아주 조금 겹친 경우의 허용 오차.
	float GroundPenetrationTolerance = 0.005f;

	// 바닥 메시를 따라 걸을 때 한 번에 오를 수 있는 높이와, 내리막에서 붙어 내려갈 수 있는 높이.
	float MaxStepHeight = 0.45f;
	float FloorSnapDistance = 0.3f;
	// 바닥 메시를 찾는 구의 반지름 = 캡슐 반지름 * 이 값. 작을수록 좁은 틈에 덜 걸리고, 클수록 작은 요철을 덜 탄다.
	float MeshFloorProbeRadiusScale = 0.6f;
	bool bWalkOnBlockingMeshes = false;
};
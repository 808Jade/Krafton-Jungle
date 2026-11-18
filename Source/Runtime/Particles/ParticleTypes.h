#pragma once

#include "Math/Vector.h"
#include "Math/Color.h"

enum class EDynamicEmitterType : uint8
{
	Sprite,
	Mesh,
	Beam,
	Ribbon,
};

enum class EParticleSortMode : uint8
{
	None,
	DistanceToView,
	ViewDepth,
};

// 스프라이트 판이 카메라에 대해 어떤 방향을 갖는지.

enum class EParticleScreenAlignment : uint8
{
	FacingCameraPosition, // 카메라 위치를 향한다. (카메라가 가까이 붙으면 뒤집히는 현상 발생)
	Square,               // 카메라 평면에 직교하는 정사각형. SizeX를 가로 세로 동일하게 사용
	Rectangle,            // 카메라 평면에 직교하는 직사각형. SizeX, SizeY를 그대로 사용
	Velocity,             // 카메라를 향하면서 이동방향에 맞춰 축을 정렬
	// TypeSpecific,
	// FacingCameraDistanceBlend, // 카메라 위치를 향하지만 일정 거리 이상이면 정사각형으로 바뀐다.
};

// AxisLock에서 고정할 월드 축.
enum class EParticleAxisLock : uint8
{
	None,
	X,
	Y,
	Z,
};

// Collision 모듈의 MaxCollisions에 도달했을 때 파티클에 하는 일.
// 순서는 편집기 표시 표(ParticleSystemDetailsPanel.cpp)와 같아야 한다.
enum class EParticleCollisionComplete : uint8
{
	Kill,            // 마지막 충돌에서 파티클을 없앤다.
	Freeze,          // 마지막 충돌 지점에 멈춰 붙는다.
	HaltCollisions,  // 마지막 충돌은 튕기고, 그 뒤로는 충돌 검사를 하지 않는다.
};

enum EParticleStates
{
	/** Ignore updates to the particle						*/
	STATE_Particle_JustSpawned = 0x02000000,
	/** Ignore updates to the particle						*/
	STATE_Particle_Freeze = 0x04000000,
	/** Ignore collision updates to the particle			*/
	STATE_Particle_IgnoreCollisions = 0x08000000,
	/**	Stop translations of the particle					*/
	STATE_Particle_FreezeTranslation = 0x10000000,
	/**	Stop rotations of the particle						*/
	STATE_Particle_FreezeRotation = 0x20000000,
	/** Combination for a single check of 'ignore' flags	*/
	STATE_Particle_CollisionIgnoreCheck = STATE_Particle_Freeze | STATE_Particle_IgnoreCollisions | STATE_Particle_FreezeTranslation | STATE_Particle_FreezeRotation,
	/** Delay collision updates to the particle				*/
	STATE_Particle_DelayCollisions = 0x40000000,
	/** Flag indicating the particle has had at least one collision	*/
	STATE_Particle_CollisionHasOccurred = 0x80000000,
	/** State mask. */
	STATE_Mask = 0xFE000000,
	/** Counter mask. */
	STATE_CounterMask = (~STATE_Mask)
};

struct FBaseParticle
{
	float OneOverMaxLifetime;
	float RelativeTime;
	FVector Location;
	FVector OldLocation;
	FVector Velocity;
	float Rotation;
	FVector BaseVelocity;
	float RotationRate;
	FVector Size;
	float BaseRotationRate;
	FVector BaseSize;
	int32 Flags;
	FLinearColor Color{ 1.0f, 1.0f, 1.0f, 1.0f };
	FLinearColor BaseColor{ 1.0f, 1.0f, 1.0f, 1.0f };
};

class UPrimitiveComponent;
class AActor;

// 파티클이 다른 컴포넌트와 부딪혔을 때 만들어지는 이벤트 정보. 위치와 방향은 모두 월드 좌표다.
// Collision 모듈의 bGenerateEvents를 켜면 충돌할 때마다 만들어져 UParticleSystemComponent::OnParticleCollide로 전달된다.
struct FParticleCollisionEvent
{
	// Collision 모듈의 EventName. 이미터마다 다른 이름을 줘서 어느 충돌인지 구분한다.
	FString EventName;
	// 충돌 시점의 이미터 시간(초)과 파티클 수명 진행 비율(0~1)
	float EmitterTime = 0.0f;
	float ParticleRelativeTime = 0.0f;
	// 이 파티클의 몇 번째 충돌인지 (1부터). 마지막 충돌의 이벤트도 만들어진다.
	int32 CollisionCount = 0;
	// 표면 위의 충돌점
	FVector Location;
	// 튕기기 전의 파티클 속도
	FVector Velocity;
	// 표면에서 파티클 쪽을 향하는 단위 법선
	FVector Normal;
	// 맞은 컴포넌트와 그 소유 액터. 이벤트를 받은 시점(같은 프레임)까지만 유효하다.
	UPrimitiveComponent* HitComponent = nullptr;
	AActor* HitActor = nullptr;
};

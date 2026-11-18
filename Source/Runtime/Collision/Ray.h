#pragma once

#include <cmath>
#include "Math/EngineMath.h"

struct FStaticMeshData;

struct FRay
{
	// 광선 시작위치
	FVector Origin;
	// 광선 방향
	FVector Direction;

	FVector PointAt(float t) const
	{
		return Origin + Direction * t;
	}
};

struct FPreparedRay
{
	FVectorRegister Origin;
	FVectorRegister InvDirection;

	explicit FPreparedRay(const FRay& Ray)
		: Origin(VectorSIMD::LoadFloat3(&Ray.Origin.X))
		, InvDirection(MakeInvDirection(Ray.Direction))
	{}

private:
	// 방향 성분이 정확히 0이면 역수가 무한대가 되고, 슬랩 검사에서 (박스 경계 - 시작점) = 0인 축이
	// 0 x 무한대 = NaN이 되어 레이가 박스를 놓친다. (축에 평행하게 쏜 레이가 대칭면 위에서 시작하는 경우)
	// 0에 가까운 성분을 같은 부호의 아주 작은 값으로 바꿔 역수를 유한하게 만든다.
	static FVectorRegister MakeInvDirection(const FVector& Direction)
	{
		constexpr float MinComponent = 1.0e-20f;
		const auto Safe = [](float V) { return std::abs(V) < MinComponent ? (std::signbit(V) ? -MinComponent : MinComponent) : V; };
		const FVector SafeDirection(Safe(Direction.X), Safe(Direction.Y), Safe(Direction.Z));
		return VectorSIMD::Reciprocal(VectorSIMD::LoadFloat3(&SafeDirection.X));
	}
};

// 월드 레이를 로컬로. 방향은 정규화하지 않는다 (t가 월드 거리로 유지되도록)
FRay ToLocalRay(const FRay& WorldRay, const FMatrix& WorldMatrix);

bool RayIntersectsAABB(const FRay& Ray, const FVector& BoxMin, const FVector& BoxMax, float& OutT);
bool RayIntersectsAABB(const FPreparedRay& PreparedRay, const FVector& BoxMin, const FVector& BoxMax, float& OutT);

bool RayIntersectsTriangle(const FRay& Ray, const FVector& v1, const FVector& v2, const FVector& v3, float& OutT);

bool RayIntersectsTriangleEdges(const FRay& Ray, const FVector& V0, const FVector& Edge1, const FVector& Edge2, float& OutT);

// OutLocalNormal을 주면 가장 가까운 교차 삼각형의 로컬 단위 법선을 채운다. 레이를 향하는 면(앞면)으로 뒤집어 돌려준다.
// 교차가 없으면 OutLocalNormal은 건드리지 않는다.
bool RayIntersectsMesh(const FRay& LocalRay, const FStaticMeshData& Mesh, float& InOutNearestT, FVector* OutLocalNormal = nullptr);

// 구 스윕 결과. Time은 입력이면 "이 시점보다 늦은 충돌은 무시"하는 상한이고, 출력이면 가장 이른 충돌 시점이다.
struct FSphereSweepHit
{
	float Time = 1.0f;                // 0~1. 이동 구간(Delta)에서의 비율
	FVector Normal;                   // 표면에서 구 중심 쪽을 향하는 단위 법선
	FVector ImpactPoint;              // 구가 닿은 표면 위의 점
	bool bStartPenetrating = false;   // 시작부터 겹쳐 있었다(Time == 0)
};

// 구(Center, Radius)를 Center → Center + Delta로 움직일 때 월드 공간 삼각형(A, B, C)에 처음 닿는 지점을 찾는다.
// InOutHit.Time보다 이른 충돌일 때만 InOutHit을 갱신하고 true를 반환한다. 삼각형은 양면으로 취급한다.
// 시작부터 겹쳐 있으면 구가 면 쪽으로 움직일 때만 Time 0 충돌로 알린다. (빠져나가는 중이면 장애물로 보지 않는다)
bool SweepSphereTriangle(const FVector& Center, const FVector& Delta, float Radius, const FVector& A, const FVector& B, const FVector& C, FSphereSweepHit& InOutHit);

// 월드 공간의 구를 메시(LocalToWorld로 배치된)에 쓸어서 가장 이른 충돌을 찾는다. 비균등 스케일도 월드 삼각형으로 정확히 처리한다.
bool SweepSphereMesh(const FMatrix& LocalToWorld, const FMatrix& WorldToLocal, const FStaticMeshData& Mesh, const FVector& Center, const FVector& Delta, float Radius, FSphereSweepHit& InOutHit);

// 로컬 법선을 월드 법선으로 바꾼다. 비균등 스케일에서도 면에 수직이 유지되도록 역전치 변환을 쓴다.
// WorldToLocal은 월드→로컬 행렬이다. 결과는 단위 길이다.
FVector LocalNormalToWorld(const FMatrix& WorldToLocal, const FVector& LocalNormal);

FVector2 WorldToScreen(const FVector& WorldPos, const FMatrix& ViewProj, int ScreenW, int ScreenH);

float DistanceToSegment(const FVector2& P, const FVector2& A, const FVector2& B);

bool RayIntersectsPlane(const FRay& Ray, const FVector& PlanePoint, const FVector& PlaneNormal, float& OutT);

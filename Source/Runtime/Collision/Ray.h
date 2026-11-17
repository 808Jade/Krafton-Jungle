#pragma once

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
		, InvDirection(VectorSIMD::Reciprocal(VectorSIMD::LoadFloat3(&Ray.Direction.X)))
	{}
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

// 로컬 법선을 월드 법선으로 바꾼다. 비균등 스케일에서도 면에 수직이 유지되도록 역전치 변환을 쓴다.
// WorldToLocal은 월드→로컬 행렬이다. 결과는 단위 길이다.
FVector LocalNormalToWorld(const FMatrix& WorldToLocal, const FVector& LocalNormal);

FVector2 WorldToScreen(const FVector& WorldPos, const FMatrix& ViewProj, int ScreenW, int ScreenH);

float DistanceToSegment(const FVector2& P, const FVector2& A, const FVector2& B);

bool RayIntersectsPlane(const FRay& Ray, const FVector& PlanePoint, const FVector& PlaneNormal, float& OutT);

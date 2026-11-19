#include "EnginePCH.h"
#include "Ray.h"

#include <algorithm>
#include "Math/EngineMath.h"
#include "Render/StaticMeshData.h"


FRay ToLocalRay(const FRay& WorldRay, const FMatrix& WorldMatrix)
{
    // ray를 로컬공간으로
    FMatrix invWorld = WorldMatrix.Inverse();
    FRay LocalRay{};
    LocalRay.Origin = invWorld.TransformPosition(WorldRay.Origin);
    LocalRay.Direction = invWorld.TransformVector(WorldRay.Direction);

    return LocalRay;
}

bool RayIntersectsAABB(const FRay& Ray, const FVector& BoxMin, const FVector& BoxMax, float& OutT)
{
	FPreparedRay PreparedRay(Ray);
    return RayIntersectsAABB(PreparedRay, BoxMin, BoxMax, OutT);
}

bool RayIntersectsAABB(const FPreparedRay& PreparedRay, const FVector& BoxMin, const FVector& BoxMax, float& OutT)
{
    FVectorRegister Boxminreg = VectorSIMD::LoadFloat3(&BoxMin.X);
    FVectorRegister Boxmaxreg = VectorSIMD::LoadFloat3(&BoxMax.X);

    FVectorRegister tX1reg = VectorSIMD::Mul(VectorSIMD::Sub(Boxminreg, PreparedRay.Origin), PreparedRay.InvDirection);
    FVectorRegister tX2reg = VectorSIMD::Mul(VectorSIMD::Sub(Boxmaxreg, PreparedRay.Origin), PreparedRay.InvDirection);

    FVectorRegister tminreg = VectorSIMD::Min(tX1reg, tX2reg);
    FVectorRegister tmaxreg = VectorSIMD::Max(tX1reg, tX2reg);

    FVectorRegister maxXY = VectorSIMD::Max(tminreg, VectorSIMD::SplatY(tminreg));
    FVectorRegister maxXYZ = VectorSIMD::Max(maxXY, VectorSIMD::SplatZ(tminreg));
    float tEnter = _mm_cvtss_f32(maxXYZ);

    FVectorRegister minXY = VectorSIMD::Min(tmaxreg, VectorSIMD::SplatY(tmaxreg));
    FVectorRegister minXYZ = VectorSIMD::Min(minXY, VectorSIMD::SplatZ(tmaxreg));
    float tExit = _mm_cvtss_f32(minXYZ);

    if (tEnter > tExit)
    {   // 충돌 안함
        return false;
    }

    if (tExit < 0.0f)
    {   // 박스가 Ray 뒤에 있을 경우
        return false;
    }

    // 광선이 내부라면 tEnter는 음수. 
    OutT = fmax(0.0f, tEnter);
    return true;
}

bool RayIntersectsTriangle(const FRay& Ray, const FVector& v1, const FVector& v2, const FVector& v3, float& OutT)
{
    return RayIntersectsTriangleEdges(Ray, v1, v2 - v1, v3 - v1, OutT);
}

bool RayIntersectsTriangleEdges(const FRay& Ray, const FVector& V0, const FVector& Edge1, const FVector& Edge2, float& OutT)
{
    constexpr float epsilon = 1e-5f;

    FVectorRegister edge1 = VectorSIMD::LoadFloat3(&Edge1.X);
    FVectorRegister edge2 = VectorSIMD::LoadFloat3(&Edge2.X);

    FVectorRegister RayVector = VectorSIMD::LoadFloat3(&Ray.Direction.X);
    const FVectorRegister rayCrossVec = VectorSIMD::Cross3(RayVector, edge2);
    float det = VectorSIMD::Dot(rayCrossVec, edge1);
    if (fabs(det) < epsilon)
    {   // 내적의 결과가 0에 가까우면 180도. 평행한 관계
        return false;
    }

    float invDet = 1.0f / det;
    // 수식: Ray.Origin - v1 = u * edge1 + v * edge2 - t * Ray.Direction
    // 1. u 구하기
    FVectorRegister s = VectorSIMD::Sub(VectorSIMD::LoadFloat3(&Ray.Origin.X), VectorSIMD::LoadFloat3(&V0.X));
    float u = invDet * VectorSIMD::Dot(s, rayCrossVec);

    if (-epsilon > u || epsilon < u - 1)
    {
        return false;
    }

    FVectorRegister sCrossE1 = VectorSIMD::Cross3(s, edge1);
    float v = invDet * VectorSIMD::Dot(RayVector, sCrossE1);

    if (-epsilon > v || epsilon < u + v - 1)
    {
        return false;
    }

    float t = invDet * VectorSIMD::Dot(edge2, sCrossE1);

    if (t > epsilon)
    {
        OutT = t;
        return true;
    }

    return false;
}

// Mesh AABB를 통과한 Ray에 삼각형 교차를 적용해 가장 가까운 거리만 반환한다.
namespace
{
    // 삼각형의 단위 법선을 레이 쪽(앞면)을 향하게 만든다. 퇴화 삼각형이면 레이 반대 방향을 쓴다.
    FVector FrontFacingTriangleNormal(const FVector& Edge1, const FVector& Edge2, const FVector& RayDirection)
    {
        FVector Normal = Edge1.Cross(Edge2);
        const float Length = Normal.Length();
        if (Length < 1e-12f)
        {
            return RayDirection.GetSafeNormal() * -1.0f;
        }
        Normal = Normal * (1.0f / Length);
        if (Normal.Dot(RayDirection) > 0.0f)
        {
            Normal = Normal * -1.0f;
        }
        return Normal;
    }
}

FVector LocalNormalToWorld(const FMatrix& WorldToLocal, const FVector& LocalNormal)
{
    // 점이 p_local = p_world * WorldToLocal(행벡터)이면 법선은 WorldToLocal을 열벡터 행렬로 곱한 값이다.
    const FVector WorldNormal(
        WorldToLocal.M[0][0] * LocalNormal.X + WorldToLocal.M[0][1] * LocalNormal.Y + WorldToLocal.M[0][2] * LocalNormal.Z,
        WorldToLocal.M[1][0] * LocalNormal.X + WorldToLocal.M[1][1] * LocalNormal.Y + WorldToLocal.M[1][2] * LocalNormal.Z,
        WorldToLocal.M[2][0] * LocalNormal.X + WorldToLocal.M[2][1] * LocalNormal.Y + WorldToLocal.M[2][2] * LocalNormal.Z);
    return WorldNormal.GetSafeNormal(1e-12f, LocalNormal);
}

bool RayIntersectsMesh(const FRay& LocalRay, const FStaticMeshData& Mesh, float& InOutNearestT, FVector* OutLocalNormal)
{
    FVector HitNormal{};
    if (!Mesh.TriangleBVH)
    {
	    FBox Box = Mesh.AABB;
	    float BoxT{};

        if (!RayIntersectsAABB(LocalRay, Box.Min, Box.Max, BoxT) ||
			BoxT >= InOutNearestT)
			return false;
    }

    bool bHit = false;
    float NearestT = InOutNearestT;

    const FPreparedRay PreparedRay(LocalRay);

    if (Mesh.TriangleBVH)
    {
        bHit = Mesh.TriangleBVH->TraceClosest(
            [&](const FBox& Bounds, float& OutEnterT)
            {
                return RayIntersectsAABB(PreparedRay, Bounds.Min, Bounds.Max, OutEnterT);
            },
            [&](const FMeshTriangleElement& Element, float& OutNearestT)
            {
                float T = InOutNearestT;
                if (RayIntersectsTriangleEdges(
                    LocalRay,
                    Element.V0,
                    Element.Edge1,
                    Element.Edge2,
                    T) &&
                    T < OutNearestT)
                {
                    OutNearestT = T;
                    // TraceClosest는 더 가까운 교차만 받아들이므로 마지막으로 받은 삼각형이 가장 가까운 삼각형이다.
                    if (OutLocalNormal)
                    {
                        HitNormal = FrontFacingTriangleNormal(Element.Edge1, Element.Edge2, LocalRay.Direction);
                    }
                    return true;
                }

                return false;
            },
            NearestT);
    }
    else
    {
        for (uint32 i = 0; i + 2 < Mesh.Indices.Num(); i += 3)
        {
            FVector vertices[3]{};	// 3 vertex
            for (uint32 j = 0; j < 3; ++j)
            {
                uint32 index = Mesh.Indices[i + j];

                vertices[j].X = Mesh.Vertices[index].Position.X;
                vertices[j].Y = Mesh.Vertices[index].Position.Y;
                vertices[j].Z = Mesh.Vertices[index].Position.Z;
            }

            float T = FLT_MAX;
            if (RayIntersectsTriangle(LocalRay, vertices[0], vertices[1], vertices[2], T) && T < NearestT)
            {
                NearestT = T;
                bHit = true;
                if (OutLocalNormal)
                {
                    HitNormal = FrontFacingTriangleNormal(vertices[1] - vertices[0], vertices[2] - vertices[0], LocalRay.Direction);
                }
            }
        }
    }

    if (bHit)
    {
        InOutNearestT = NearestT;
        if (OutLocalNormal)
        {
            *OutLocalNormal = HitNormal;
        }
    }
    return bHit;
}

FVector2 WorldToScreen(const FVector& WorldPos, const FMatrix& ViewProj, int ScreenW, int ScreenH)
{
    FVector4 clip = FVector4(WorldPos.X, WorldPos.Y, WorldPos.Z, 1.0f) * ViewProj;

    if (clip.W < 0.0001f)
        return FVector2(-FLT_MAX, -FLT_MAX);

    float ndcX = clip.X / clip.W;
    float ndcY = clip.Y / clip.W;

    FVector2 result;
    result.X = (ndcX * 0.5f + 0.5f) * ScreenW;
    result.Y = (1.0f - (ndcY * 0.5f + 0.5f)) * ScreenH;   // Y 뒤집기
    return result;
}

float DistanceToSegment(const FVector2& P, const FVector2& A, const FVector2& B)
{
    FVector2 seg = B - A;
    float segLenSq = seg.X * seg.X + seg.Y * seg.Y;

    if (segLenSq < 1e-6f)
    {
        FVector2 d = P - A;
        return sqrtf(d.X * d.X + d.Y * d.Y);
    }

    FVector2 toP = P - A;
    float t = (toP.X * seg.X + toP.Y * seg.Y) / segLenSq;

    t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);

    FVector2 closest = A + seg * t;
    FVector2 diff = P - closest;
    return sqrtf(diff.X * diff.X + diff.Y * diff.Y);
}

namespace
{
    // 삼각형(A, B, C) 위에서 P와 가장 가까운 점. (Ericson, Real-Time Collision Detection)
    FVector ClosestPointOnTriangle(const FVector& P, const FVector& A, const FVector& B, const FVector& C)
    {
        const FVector AB = B - A;
        const FVector AC = C - A;
        const FVector AP = P - A;
        const float D1 = AB.Dot(AP);
        const float D2 = AC.Dot(AP);
        if (D1 <= 0.0f && D2 <= 0.0f) return A;

        const FVector BP = P - B;
        const float D3 = AB.Dot(BP);
        const float D4 = AC.Dot(BP);
        if (D3 >= 0.0f && D4 <= D3) return B;

        const float VC = D1 * D4 - D3 * D2;
        if (VC <= 0.0f && D1 >= 0.0f && D3 <= 0.0f)
        {
            return A + AB * (D1 / (D1 - D3));
        }

        const FVector CP = P - C;
        const float D5 = AB.Dot(CP);
        const float D6 = AC.Dot(CP);
        if (D6 >= 0.0f && D5 <= D6) return C;

        const float VB = D5 * D2 - D1 * D6;
        if (VB <= 0.0f && D2 >= 0.0f && D6 <= 0.0f)
        {
            return A + AC * (D2 / (D2 - D6));
        }

        const float VA = D3 * D6 - D5 * D4;
        if (VA <= 0.0f && (D4 - D3) >= 0.0f && (D5 - D6) >= 0.0f)
        {
            return B + (C - B) * ((D4 - D3) / ((D4 - D3) + (D5 - D6)));
        }

        const float Denom = 1.0f / (VA + VB + VC);
        return A + AB * (VB * Denom) + AC * (VC * Denom);
    }

    // 삼각형이 놓인 평면 위의 점 P가 삼각형 안에 있는지 (경계 포함, 작은 여유)
    bool PointInTriangle(const FVector& P, const FVector& A, const FVector& B, const FVector& C)
    {
        const FVector V0 = C - A;
        const FVector V1 = B - A;
        const FVector V2 = P - A;
        const float D00 = V0.Dot(V0);
        const float D01 = V0.Dot(V1);
        const float D02 = V0.Dot(V2);
        const float D11 = V1.Dot(V1);
        const float D12 = V1.Dot(V2);
        const float Denom = D00 * D11 - D01 * D01;
        if (std::abs(Denom) < 1e-20f) return false;
        const float InvDenom = 1.0f / Denom;
        const float U = (D11 * D02 - D01 * D12) * InvDenom;
        const float V = (D00 * D12 - D01 * D02) * InvDenom;
        constexpr float Eps = 1e-5f;
        return U >= -Eps && V >= -Eps && U + V <= 1.0f + Eps;
    }
}

bool SweepSphereTriangle(const FVector& Center, const FVector& Delta, float Radius, const FVector& A, const FVector& B, const FVector& C, FSphereSweepHit& InOutHit)
{
    constexpr float Eps = 1e-8f;
    if (Radius <= 0.0f) return false;

    FVector N = (B - A).Cross(C - A);
    const float NormalLength = N.Length();
    if (NormalLength < Eps) return false;
    N = N * (1.0f / NormalLength);

    // 구가 있는 쪽을 앞면으로 본다. (삼각형은 양면)
    float DistToPlane = (Center - A).Dot(N);
    if (DistToPlane < 0.0f)
    {
        N = N * -1.0f;
        DistToPlane = -DistToPlane;
    }

    const float RadiusSq = Radius * Radius;
    bool bHit = false;
    const auto Accept = [&](float Time, const FVector& Normal, const FVector& Point, bool bEmbedded)
    {
        InOutHit.Time = Time;
        InOutHit.Normal = Normal;
        InOutHit.ImpactPoint = Point;
        InOutHit.bStartPenetrating = bEmbedded;
        bHit = true;
    };

    // 1) 시작부터 구가 삼각형과 겹쳐 있는 경우
    const FVector Closest = ClosestPointOnTriangle(Center, A, B, C);
    const FVector ToCenter = Center - Closest;
    const float DistSq = ToCenter.SizeSquared();
    if (DistSq < RadiusSq)
    {
        const float Dist = std::sqrt(DistSq);
        const FVector Normal = Dist > 1e-6f ? ToCenter * (1.0f / Dist) : N;
        if (Delta.Dot(Normal) < 0.0f && 0.0f < InOutHit.Time)
        {
            Accept(0.0f, Normal, Closest, true);
            return true;
        }
        return false; // 빠져나가는 중이면 이 삼각형은 막지 않는다.
    }

    // 2) 면: 구가 평면에서 반경만큼 떨어지는 순간, 접촉점이 삼각형 안에 있는가
    const float NormalSpeed = Delta.Dot(N);
    if (NormalSpeed < -Eps && DistToPlane >= Radius)
    {
        const float Time = (Radius - DistToPlane) / NormalSpeed;
        if (Time < InOutHit.Time)
        {
            const FVector Point = Center + Delta * Time - N * Radius;
            if (PointInTriangle(Point, A, B, C))
            {
                Accept(Time, N, Point, false);
            }
        }
    }

    // 3) 모서리: 구 중심이 모서리를 지나는 직선에서 반경만큼 떨어지는 순간(원기둥), 접촉점이 선분 안에 있는가
    const auto SweepEdge = [&](const FVector& P0, const FVector& P1)
    {
        const FVector Edge = P1 - P0;
        const float EdgeLenSq = Edge.Dot(Edge);
        if (EdgeLenSq < Eps) return;

        const FVector S = Center - P0;
        const float DeltaAlong = Delta.Dot(Edge);
        const float StartAlong = S.Dot(Edge);
        const FVector DeltaPerp = Delta - Edge * (DeltaAlong / EdgeLenSq);
        const FVector StartPerp = S - Edge * (StartAlong / EdgeLenSq);

        const float QA = DeltaPerp.Dot(DeltaPerp);
        if (QA < Eps) return; // 모서리와 나란히 움직인다.
        const float QB = 2.0f * StartPerp.Dot(DeltaPerp);
        const float QC = StartPerp.Dot(StartPerp) - RadiusSq;
        if (QC <= 0.0f) return; // 이미 직선과 반경 안 (접촉점이 선분 밖이거나 위에서 겹침 처리됨)
        const float Disc = QB * QB - 4.0f * QA * QC;
        if (Disc < 0.0f) return;
        const float Time = (-QB - std::sqrt(Disc)) / (2.0f * QA);
        if (Time < 0.0f || Time >= InOutHit.Time) return;

        const float U = (StartAlong + DeltaAlong * Time) / EdgeLenSq;
        if (U < 0.0f || U > 1.0f) return;

        const FVector Point = P0 + Edge * U;
        const FVector At = Center + Delta * Time;
        Accept(Time, (At - Point).Normalized(), Point, false);
    };
    SweepEdge(A, B);
    SweepEdge(B, C);
    SweepEdge(C, A);

    // 4) 꼭짓점: 꼭짓점을 중심으로 한 반경 구에 중심의 이동 경로(선분)가 닿는 순간
    const auto SweepVertex = [&](const FVector& V)
    {
        const FVector S = Center - V;
        const float QA = Delta.Dot(Delta);
        if (QA < Eps) return;
        const float QB = 2.0f * S.Dot(Delta);
        const float QC = S.Dot(S) - RadiusSq;
        if (QC <= 0.0f) return;
        const float Disc = QB * QB - 4.0f * QA * QC;
        if (Disc < 0.0f) return;
        const float Time = (-QB - std::sqrt(Disc)) / (2.0f * QA);
        if (Time < 0.0f || Time >= InOutHit.Time) return;

        const FVector At = Center + Delta * Time;
        Accept(Time, (At - V).Normalized(), V, false);
    };
    SweepVertex(A);
    SweepVertex(B);
    SweepVertex(C);

    return bHit;
}

bool SweepSphereMesh(const FMatrix& LocalToWorld, const FMatrix& WorldToLocal, const FStaticMeshData& Mesh, const FVector& Center, const FVector& Delta, float Radius, FSphereSweepHit& InOutHit)
{
    // 구가 쓸고 지나가는 영역의 월드 AABB를 로컬 AABB로 바꿔 후보 삼각형만 추린다. (회전·비균등 스케일에 대해 보수적)
    const FVector End = Center + Delta;
    const FVector WorldMin(std::min(Center.X, End.X) - Radius, std::min(Center.Y, End.Y) - Radius, std::min(Center.Z, End.Z) - Radius);
    const FVector WorldMax(std::max(Center.X, End.X) + Radius, std::max(Center.Y, End.Y) + Radius, std::max(Center.Z, End.Z) + Radius);

    FVector LocalMin(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector LocalMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (int32 Corner = 0; Corner < 8; ++Corner)
    {
        const FVector WorldCorner((Corner & 1) ? WorldMax.X : WorldMin.X, (Corner & 2) ? WorldMax.Y : WorldMin.Y, (Corner & 4) ? WorldMax.Z : WorldMin.Z);
        const FVector LocalCorner = WorldToLocal.TransformPosition(WorldCorner);
        LocalMin = FVector(std::min(LocalMin.X, LocalCorner.X), std::min(LocalMin.Y, LocalCorner.Y), std::min(LocalMin.Z, LocalCorner.Z));
        LocalMax = FVector(std::max(LocalMax.X, LocalCorner.X), std::max(LocalMax.Y, LocalCorner.Y), std::max(LocalMax.Z, LocalCorner.Z));
    }
    const auto Overlaps = [&](const FBox& Box)
    {
        return Box.Min.X <= LocalMax.X && Box.Max.X >= LocalMin.X
            && Box.Min.Y <= LocalMax.Y && Box.Max.Y >= LocalMin.Y
            && Box.Min.Z <= LocalMax.Z && Box.Max.Z >= LocalMin.Z;
    };

    bool bHit = false;
    const auto TestTriangle = [&](const FVector& L0, const FVector& L1, const FVector& L2)
    {
        const FVector A = LocalToWorld.TransformPosition(L0);
        const FVector B = LocalToWorld.TransformPosition(L1);
        const FVector C = LocalToWorld.TransformPosition(L2);
        bHit |= SweepSphereTriangle(Center, Delta, Radius, A, B, C, InOutHit);
    };

    if (Mesh.TriangleBVH)
    {
        Mesh.TriangleBVH->Query(
            [&](const FBox& Box) { return Overlaps(Box); },
            [&](const FMeshTriangleElement& Element)
            {
                TestTriangle(Element.V0, Element.V0 + Element.Edge1, Element.V0 + Element.Edge2);
            });
    }
    else
    {
        for (uint32 i = 0; i + 2 < Mesh.Indices.Num(); i += 3)
        {
            const FVector& L0 = Mesh.Vertices[Mesh.Indices[i]].Position;
            const FVector& L1 = Mesh.Vertices[Mesh.Indices[i + 1]].Position;
            const FVector& L2 = Mesh.Vertices[Mesh.Indices[i + 2]].Position;
            const FVector TriMin(std::min({ L0.X, L1.X, L2.X }), std::min({ L0.Y, L1.Y, L2.Y }), std::min({ L0.Z, L1.Z, L2.Z }));
            const FVector TriMax(std::max({ L0.X, L1.X, L2.X }), std::max({ L0.Y, L1.Y, L2.Y }), std::max({ L0.Z, L1.Z, L2.Z }));
            if (Overlaps(FBox{ TriMin, TriMax }))
            {
                TestTriangle(L0, L1, L2);
            }
        }
    }
    return bHit;
}

bool RayIntersectsPlane(const FRay& Ray, const FVector& PlanePoint, const FVector& PlaneNormal, float& OutT)
{
    float denom = Ray.Direction.Dot(PlaneNormal);

    if (fabsf(denom) < 1e-6f)
        return false;

    OutT = (PlanePoint - Ray.Origin).Dot(PlaneNormal) / denom;

    return OutT >= 0.0f;
}

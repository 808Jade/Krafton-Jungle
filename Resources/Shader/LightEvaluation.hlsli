#ifndef LIGHT_EVALUATION_INCLUDED
#define LIGHT_EVALUATION_INCLUDED

#include "LightData.hlsli"

#define SHADOW_FILTER_PCSS 4u

// hwang의 Ambient/Lambert/Blinn-Phong 식을 Forward와 Deferred가 공유한다.
struct FLightResult
{
    float3 Diffuse;
    float3 Specular;
};

float CalculateDistanceAttenuation(float DistanceSquared, float RadiusSquared)
{
    if (RadiusSquared <= 0.0f || DistanceSquared >= RadiusSquared || DistanceSquared < 1.0e-8f)
        return 0.0f;
    float RadiusFade = saturate(1.0f - DistanceSquared / RadiusSquared);
    return RadiusFade * RadiusFade / (1.0f + DistanceSquared);
}

float CalculateConeAttenuation(float CosAngle, float CosInner, float CosOuter)
{
    if (CosAngle <= CosOuter) return 0.0f;
    return saturate((CosAngle - CosOuter) / max(CosInner - CosOuter, 1.0e-8f));
}

void AccumulateLight(inout FLightResult Result, float3 N, float3 V, float3 L,
    float3 Radiance, float3 SpecularColor, float Shininess, bool bIncludeSpecular)
{
    float NoL = saturate(dot(N, L));
    Result.Diffuse += Radiance * NoL;
    if (bIncludeSpecular && NoL > 0.0f)
    {
        float3 H = L + V;
        H *= rsqrt(max(dot(H, H), 1.0e-8f));
        Result.Specular += Radiance * SpecularColor * pow(saturate(dot(N, H)), max(Shininess, 1.0f));
    }
}

float CalculateLinearDepth(float Z, float Near, float Far)
{
    return (Near * Far) / (Far - Z * (Far - Near));
}

float2 CalculateShadowDepthSlope(float3 N, float3 Coord, float CoordW, row_major float4x4 ViewProjection)
{
    float3 W = float3(ViewProjection[0][3], ViewProjection[1][3], ViewProjection[2][3]);
    float3 Jx = (float3(ViewProjection[0][0], ViewProjection[1][0], ViewProjection[2][0]) - Coord.x * W) / CoordW;
    float3 Jy = (float3(ViewProjection[0][1], ViewProjection[1][1], ViewProjection[2][1]) - Coord.y * W) / CoordW;
    float3 Jz = (float3(ViewProjection[0][2], ViewProjection[1][2], ViewProjection[2][2]) - Coord.z * W) / CoordW;
    float3 XBasis = cross(Jy, N);
    float3 YBasis = cross(N, Jx);
    float Determinant = dot(Jx, XBasis);
    return abs(Determinant) > 1.0e-12f
        ? float2(dot(Jz, XBasis), dot(Jz, YBasis)) / Determinant : 0.0f;
}

float CalculateShadowGrid(float3 P, float3 N, float3 LightDirection, Texture2DArray Atlas, uint AtlasWidth, uint AtlasHeight, FLightShadowRenderData Shadow, SamplerState PointSampler, SamplerComparisonState ShadowSampler, float bReceiveShadow, uint PcfMode, float Bias, float FilterRadiusTexels)
{
    [branch]
    if (bReceiveShadow < 0.5f)
        return 1.0f;
    
    // LightDirection은 표면에서 광원을 향하는 방향이다.
    // Bias는 빛 쪽으로, SlopeBias는 법선 쪽으로 밀되 빛과 비스듬할수록(sin) 크게 민다.
    float3 L = normalize(LightDirection);
    float NoL = saturate(dot(N, L));
    float SinTheta = sqrt(saturate(1.0 - NoL * NoL));
    float3 BiasedP = P + L * Shadow.Bias + N * (Shadow.SlopeBias * SinTheta);

    float Result = 1.0;

    float4 CoordPos = mul(float4(BiasedP, 1.0), Shadow.ViewProjection);
    CoordPos.xyz /= CoordPos.w == 0.0 ? 0.0001 : CoordPos.w;
        
    float CenterAtlasU = CoordPos.x * 0.5 + 0.5;
    CenterAtlasU = (Shadow.AtlasRect.x + CenterAtlasU * Shadow.AtlasRect.z) / AtlasWidth;

    float CenterAtlasV = -CoordPos.y * 0.5 + 0.5;
    CenterAtlasV = (Shadow.AtlasRect.y + CenterAtlasV * Shadow.AtlasRect.w) / AtlasHeight;
            
    float CenterDepth = CoordPos.z;

    if (CoordPos.w > 0.0 && CoordPos.x >= -1.0 && CoordPos.x <= 1.0 && CoordPos.y >= -1.0 && CoordPos.y <= 1.0 && CoordPos.z >= 0 && CoordPos.z <= 1)
    {
        // 표면 평면의 조명 공간 깊이 기울기. 미분 명령 없이 VS/PS에서 공통으로 계산한다.
        float2 DepthSlope = CalculateShadowDepthSlope(N, CoordPos.xyz, CoordPos.w, Shadow.ViewProjection);
        float2 TexelNdc = float2(2.0f / Shadow.AtlasRect.z, -2.0f / Shadow.AtlasRect.w);
        float FilterBias = 0.5f * dot(abs(DepthSlope), abs(TexelNdc));

        float2 AtlasURange = float2((Shadow.AtlasRect.x + 0.5) / AtlasWidth, (Shadow.AtlasRect.x + Shadow.AtlasRect.z - 0.5) / AtlasWidth);
        float2 AtlasVRange = float2((Shadow.AtlasRect.y + 0.5) / AtlasHeight, (Shadow.AtlasRect.y + Shadow.AtlasRect.w - 0.5) / AtlasHeight);
            
        float2 Texel = float2(1.0 / (float) AtlasWidth, 1.0 / (float) AtlasHeight);
        
        uint KernelSize = FilterRadiusTexels >= 0.0f ? 7 : (PcfMode == 0 ? 1 : min(PcfMode + 2, 5));
        float HalfWidth = 0.5f * (KernelSize - 1);
        float Sum = 0;
        int SampleCount = 0;
        [loop]
        for (uint i = 0; i < KernelSize; i++)
        {
            [loop]
            for (uint j = 0; j < KernelSize; j++)
            {
                float2 TapOffset = float2((float)i, (float)j) - HalfWidth;
                if (FilterRadiusTexels >= 0.0f)
                    TapOffset *= FilterRadiusTexels / max(HalfWidth, 0.5f);
                float AtlasU = CenterAtlasU + Texel.x * TapOffset.x;
                float AtlasV = CenterAtlasV + Texel.y * TapOffset.y;

                if (AtlasU < AtlasURange.x || AtlasU > AtlasURange.y || AtlasV < AtlasVRange.x || AtlasV > AtlasVRange.y)
                    continue;

                float TapDepth = CenterDepth + dot(DepthSlope, TapOffset * TexelNdc) - FilterBias;
                Sum += Atlas.SampleCmpLevelZero(ShadowSampler, float3(AtlasU, AtlasV, Shadow.AtlasIndex), TapDepth).r;
                SampleCount++;
            }
        }

        if (SampleCount > 0)
        {
            Result = Sum / (float) SampleCount;
            // 0.5를 기준으로 대비를 키워 반그림자 폭을 좁힌다. Sharpen 1이면 기울기 8배.
            Result = saturate((Result - 0.5) * (1.0 + Shadow.Sharpen * 7.0) + 0.5);
        }
    }
    
    return Result;
}

float CalculateShadow(float3 P, float3 N, float3 LightDirection, Texture2DArray Atlas, uint AtlasWidth, uint AtlasHeight, FLightShadowRenderData Shadow, SamplerState PointSampler, SamplerComparisonState ShadowSampler, float bReceiveShadow, uint PcfMode, float Bias)
{
    // Local lights and Gouraud keep the existing 5x5 grid when PCSS is selected.
    uint GridMode = PcfMode == SHADOW_FILTER_PCSS ? 3u : PcfMode;
    return CalculateShadowGrid(P, N, LightDirection, Atlas, AtlasWidth, AtlasHeight, Shadow,
        PointSampler, ShadowSampler, bReceiveShadow, GridMode, Bias, -1.0f);
}

// Directional cascade depth is linear; return the mean blocker-to-receiver depth gap.
float FindDirectionalBlockers(float3 P, float3 N, Texture2DArray Atlas, uint AtlasWidth, uint AtlasHeight,
    FLightShadowRenderData Shadow, SamplerState PointSampler, float Bias, out uint BlockerCount)
{
    float4 Coord = mul(float4(P + N * Bias, 1.0f), Shadow.ViewProjection); // 월드 좌표를 light ndc로
    float3 Projected = Coord.xyz / (Coord.w == 0.0f ? 0.0001f : Coord.w);
    BlockerCount = 0;
    if (Coord.w <= 0.0f || any(abs(Projected.xy) > 1.0f) || Projected.z < 0.0f || Projected.z > 1.0f) // shadow map에 투영되는 영역 밖에 있다면 종료
        return 0.0f;

    float2 TilePixel = float2((Projected.x * 0.5f + 0.5f) * Shadow.AtlasRect.z,
                              (-Projected.y * 0.5f + 0.5f) * Shadow.AtlasRect.w); // NDC 좌표를 Shadow Atlas의 타일 좌표로 변환
    float2 DepthSlope = CalculateShadowDepthSlope(N, Projected, Coord.w, Shadow.ViewProjection);
    float2 TexelNdc = float2(2.0f / Shadow.AtlasRect.z, -2.0f / Shadow.AtlasRect.w);
    float FilterBias = 0.5f * dot(abs(DepthSlope), abs(TexelNdc));
    float MinBlockerGap = 0.5f * Shadow.WorldUnitsPerTexel / max(Shadow.Far - Shadow.Near, 1.0e-6f);
    float DepthGapSum = 0.0f;
    // Search both near the receiver and across the widest supported penumbra.
    [loop]
    for (int Ring = 0; Ring < 2; ++Ring)
    {
        if (Ring != 0 && BlockerCount > 0)
            break;
        float Step = Ring == 0 ? 4.0f : 32.0f;
        [loop]
        for (int Y = -2; Y <= 2; ++Y)
        {
            [loop]
            for (int X = -2; X <= 2; ++X)
            {
                if (Ring != 0 && X == 0 && Y == 0)
                    continue;
                float2 Offset = float2(X, Y) * Step;
                float2 SamplePixel = TilePixel + Offset;
                if (any(SamplePixel < 0.5f) || any(SamplePixel > Shadow.AtlasRect.zw - 0.5f))
                    continue;
                float2 Uv = (Shadow.AtlasRect.xy + SamplePixel) / float2(AtlasWidth, AtlasHeight);
                float Depth = Atlas.SampleLevel(PointSampler, float3(Uv, Shadow.AtlasIndex), 0).r;
                float TapReceiverDepth = Projected.z + dot(DepthSlope, Offset * TexelNdc) - FilterBias;
                float DepthGap = TapReceiverDepth - Depth;
                if (DepthGap > MinBlockerGap)
                {
                    DepthGapSum += DepthGap;
                    ++BlockerCount;
                }
            }
        }
    }
    return BlockerCount > 0 ? DepthGapSum / BlockerCount : 0.0f;
}

float CalculateDirectionalPcssRadiusTexels(float AverageDepthGap,
    FLightShadowRenderData Shadow, float AngularRadius)
{
    float GapWorld = AverageDepthGap * (Shadow.Far - Shadow.Near);
    float RadiusWorld = GapWorld * tan(AngularRadius);
    return clamp(RadiusWorld / max(Shadow.WorldUnitsPerTexel, 1.0e-6f), 0.5f, 64.0f);
}

float CalculateDirectionalShadowPCSS(float3 P, float3 N, float3 LightDirection, Texture2DArray Atlas,
    uint AtlasWidth, uint AtlasHeight, FLightShadowRenderData Shadow, SamplerState PointSampler,
    SamplerComparisonState ShadowSampler, float bReceiveShadow, uint PcfMode, float Bias, float AngularRadius)
{
    if (PcfMode != SHADOW_FILTER_PCSS || bReceiveShadow < 0.5f || AngularRadius <= 0.0f || Shadow.WorldUnitsPerTexel <= 0.0f)
        return CalculateShadow(P, N, LightDirection, Atlas, AtlasWidth, AtlasHeight, Shadow,
            PointSampler, ShadowSampler, bReceiveShadow, PcfMode == SHADOW_FILTER_PCSS ? 0u : PcfMode, Bias);

    uint BlockerCount;
    float AverageDepthGap = FindDirectionalBlockers(P, N, Atlas, AtlasWidth, AtlasHeight,
        Shadow, PointSampler, Bias, BlockerCount);
    if (BlockerCount == 0u)
        return CalculateShadow(P, N, LightDirection, Atlas, AtlasWidth, AtlasHeight, Shadow,
            PointSampler, ShadowSampler, bReceiveShadow, 0u, Bias);

    float RadiusTexels = CalculateDirectionalPcssRadiusTexels(AverageDepthGap, Shadow, AngularRadius);
    return CalculateShadowGrid(P, N, LightDirection, Atlas, AtlasWidth, AtlasHeight, Shadow,
        PointSampler, ShadowSampler, bReceiveShadow, 3u, Bias, RadiusTexels);
}

#endif

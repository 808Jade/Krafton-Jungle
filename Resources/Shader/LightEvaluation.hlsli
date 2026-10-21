#ifndef LIGHT_EVALUATION_INCLUDED
#define LIGHT_EVALUATION_INCLUDED

#include "LightData.hlsli"

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

float CalculateShadow(float3 P, float3 N, float3 LightDirection, Texture2DArray Atlas, uint AtlasWidth, uint AtlasHeight, FLightShadowRenderData Shadow, SamplerState PointSampler, SamplerComparisonState ShadowSampler, float bReceiveShadow, uint PcfMode)
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
    
    float EdgeEpsilonX = 2.0f / Shadow.AtlasRect.z;
    float EdgeEpsilonY = 2.0f / Shadow.AtlasRect.w;
    
    if (CoordPos.w > 0.0 && CoordPos.x >= -1.0 - EdgeEpsilonX && CoordPos.x <= 1.0 + EdgeEpsilonX && CoordPos.y >= -1.0 - EdgeEpsilonY && CoordPos.y <= 1.0 + EdgeEpsilonY && CoordPos.z >= 0 && CoordPos.z <= 1)
    {
        float2 AtlasURange = float2((Shadow.AtlasRect.x + 0.5) / AtlasWidth, (Shadow.AtlasRect.x + Shadow.AtlasRect.z - 0.5) / AtlasWidth);
        float2 AtlasVRange = float2((Shadow.AtlasRect.y + 0.5) / AtlasHeight, (Shadow.AtlasRect.y + Shadow.AtlasRect.w - 0.5) / AtlasHeight);
            
        float2 Texel = float2(1.0 / (float) AtlasWidth, 1.0 / (float) AtlasHeight);
        
        uint KernelSize = PcfMode == 0 ? 1 : min(PcfMode + 2, 5);
        float HalfWidth = 0.5f * (KernelSize - 1);
        float Sum = 0;
        int SampleCount = 0;
        [loop]
        for (uint i = 0; i < KernelSize; i++)
        {
            [loop]
            for (uint j = 0; j < KernelSize; j++)
            {
                float AtlasU = clamp(CenterAtlasU + Texel.x * (i - HalfWidth), AtlasURange.x, AtlasURange.y);
                float AtlasV = clamp(CenterAtlasV + Texel.y * (j - HalfWidth), AtlasVRange.x, AtlasVRange.y);
                
                Sum += Atlas.SampleCmpLevelZero(ShadowSampler, float3(AtlasU, AtlasV, Shadow.AtlasIndex), CenterDepth).r;
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

#endif

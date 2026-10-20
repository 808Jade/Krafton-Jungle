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
    
    const float Bias = 0.01;
    
    float Result = 1.0;

    float4 CoordPos = mul(float4(P + N * Bias, 1.0), Shadow.ViewProjection);
    CoordPos.xyz /= CoordPos.w == 0.0 ? 0.0001 : CoordPos.w;
        
    float CenterAtlasU = CoordPos.x * 0.5 + 0.5;
    CenterAtlasU = (Shadow.AtlasRect.x + CenterAtlasU * Shadow.AtlasRect.z) / AtlasWidth;

    float CenterAtlasV = -CoordPos.y * 0.5 + 0.5;
    CenterAtlasV = (Shadow.AtlasRect.y + CenterAtlasV * Shadow.AtlasRect.w) / AtlasHeight;
            
    float CenterDepth = CoordPos.z;
    
    if (CoordPos.w > 0.0 && CoordPos.x >= -1.0 && CoordPos.x <= 1.0 && CoordPos.y >= -1.0 && CoordPos.y <= 1.0 && CoordPos.z >= 0 && CoordPos.z <= 1)
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
                float AtlasU = CenterAtlasU + Texel.x * (i - HalfWidth);
                float AtlasV = CenterAtlasV + Texel.y * (j - HalfWidth);
                    
                if (AtlasU < AtlasURange.x || AtlasU > AtlasURange.y || AtlasV < AtlasVRange.x || AtlasV > AtlasVRange.y)
                {
                    continue;
                }
                
                Sum += Atlas.SampleCmpLevelZero(ShadowSampler, float3(AtlasU, AtlasV, Shadow.AtlasIndex), CenterDepth).r;
                SampleCount++;
            }
        }
            
        if (SampleCount > 0)
        {
            Result = Sum / (float) SampleCount;
        }
    }
    
    return Result;
}

#endif

#ifndef LIGHT_EVALUATION_INCLUDED
#define LIGHT_EVALUATION_INCLUDED

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

float CalculateShadow(float3 P, float3 N, float3 LightDirection, float4x4 VP, float4 AtlasRect, Texture2DArray Atlas, uint AtlasWidth, uint AtlasHeight, uint AtlasIndex, SamplerComparisonState Sampler)
{
    float Shadow = 1.0;
    float Bias = 0.05;

    float4 CoordPos = mul(float4(P + N * Bias, 1.0), VP);
    if (CoordPos.w > 0)
    {
        CoordPos.xyz /= CoordPos.w;
        if (CoordPos.x >= -1.0 && CoordPos.x <= 1.0 && CoordPos.y >= -1.0 && CoordPos.y <= 1.0 && CoordPos.z >= 0 && CoordPos.z <= 1)
        {
            float2 AtlasURange = float2((AtlasRect.x + 0.5) / AtlasWidth, (AtlasRect.x + AtlasRect.z - 0.5) / AtlasWidth);
            float2 AtlasVRange = float2((AtlasRect.y + 0.5) / AtlasHeight, (AtlasRect.y + AtlasRect.w - 0.5) / AtlasHeight);
            
            float CenterAtlasU = CoordPos.x * 0.5 + 0.5;
            CenterAtlasU = (AtlasRect.x + CenterAtlasU * AtlasRect.z) / AtlasWidth;

            float CenterAtlasV = -CoordPos.y * 0.5 + 0.5;
            CenterAtlasV = (AtlasRect.y + CenterAtlasV * AtlasRect.w) / AtlasHeight;
            
#if 0
            // PCF 적용
            float2 Texel = float2(1.0 / (float) AtlasWidth, 1.0 / (float) AtlasHeight);
            
            float Sum = 0.0;
            float SampleCount = 0.0;
            for (int i = -1; i <= 1; i++)
            {
                for (int j = -1; j <= 1; j++)
                {
                    float AtlasU = CenterAtlasU + (Texel.x * i);
                    float AtlasV = CenterAtlasV + (Texel.y * j);
                    
                    if (AtlasU < AtlasURange.x || AtlasU > AtlasURange.y || AtlasV < AtlasVRange.x || AtlasV > AtlasVRange.y)
                    {
                        continue;
                    }

                    Sum += Atlas.SampleCmpLevelZero(Sampler, float3(AtlasU, AtlasV, (float) AtlasIndex), CoordPos.z).r;
                    SampleCount += 1.0;
                }
            }
            
            Shadow = Sum / SampleCount;
#else
            Shadow = Atlas.SampleCmpLevelZero(Sampler, float3(CenterAtlasU, CenterAtlasV, AtlasIndex), CoordPos.z).r;
#endif
        }
    }
    
    return Shadow;
}

#endif

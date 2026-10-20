Texture2DArray<float> ShadowAtlas : register(t0);
SamplerState PreviewSampler : register(s0);

cbuffer ShadowPreviewConstants : register(b0)
{
    uint AtlasIndex;
    float Contrast;
    uint2 Padding;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR0;
    float2 UV : TEXCOORD0;
};

float4 mainPS(PSInput Input) : SV_Target
{
    float Depth = ShadowAtlas.SampleLevel(PreviewSampler, float3(Input.UV, AtlasIndex), 0).r;
    // Keep the entire preview red, but soften the clear-depth background and
    // expand the small depth differences near 1 in perspective shadow maps.
    float Red = 0.04f + 0.41f * pow(saturate(Depth), Contrast);
    return float4(Red, 0.0f, 0.0f, 1.0f) * Input.Color;
}

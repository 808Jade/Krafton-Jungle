#pragma pack_matrix(row_major)

cbuffer ParticleMeshConstants : register(b3)
{
    float4x4 ViewProjection;
    float4 BaseColor;
    
    uint bOpaque;
    uint3 Padding;
};

Texture2D BaseTexture : register(t0);
SamplerState BaseSampler : register(s0);

struct VS_INPUT
{
    float3 Position : POSITION0;
    float2 UV : TEXCOORD0;

    float4 Transform0 : INSTANCE_TRANSFORM0;
    float4 Transform1 : INSTANCE_TRANSFORM1;
    float4 Transform2 : INSTANCE_TRANSFORM2;
    float4 Transform3 : INSTANCE_TRANSFORM3;

    float4 Color : INSTANCE_COLOR0;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
};

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;

    float4x4 World = float4x4(
        Input.Transform0,
        Input.Transform1,
        Input.Transform2,
        Input.Transform3);

    float4 WorldPosition =
        mul(float4(Input.Position, 1.0f), World);

    Output.Position = mul(WorldPosition, ViewProjection);
    Output.UV = Input.UV;
    Output.Color = Input.Color;

    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_Target
{
    float4 Color =
        BaseTexture.Sample(BaseSampler, Input.UV)
        * BaseColor
        * Input.Color;

    if (bOpaque != 0)
        Color.a = 1.0f;

    return Color;
}

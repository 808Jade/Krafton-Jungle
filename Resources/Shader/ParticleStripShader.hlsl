#pragma pack_matrix(row_major)

cbuffer ParticleStripConstants : register(b3)
{
    float4x4 ViewProjection;

    float4 MaterialColor;

    uint UseTexture;
    float3 StripPadding;
};

Texture2D StripTexture : register(t0);
SamplerState StripSampler : register(s0);

struct VS_INPUT
{
    float3 Position : POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
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

    // CPU에서 이미 월드 공간 정점을 생성했다.
    Output.Position = mul(
        float4(Input.Position, 1.0f),
        ViewProjection);

    Output.UV = Input.UV;
    Output.Color = Input.Color;

    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_Target
{
    float4 TextureColor = float4(1, 1, 1, 1);

    if (UseTexture != 0)
    {
        TextureColor = StripTexture.Sample(
            StripSampler,
            Input.UV);
    }

    return TextureColor * Input.Color * MaterialColor;
}
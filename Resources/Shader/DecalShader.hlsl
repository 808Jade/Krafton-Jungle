#pragma pack_matrix(row_major)

cbuffer Viewconstants : register(b0)
{
    matrix VP;
};

cbuffer Worldconstants : register(b2)
{
    matrix World;
};

cbuffer DecalConstants : register(b3)
{
    matrix WorldToDecal;
};

struct VS_INPUT
{
    float3 p : POSITION;
    float3 n : NORMAL;
    float4 tangent : TANGENT;
    float2 t : TEXCOORD;
    float4 c : COLOR;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
};

Texture2D g_txDecal : register(t0);
SamplerState g_Sample : register(s0);

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;
    
    float4 WorldPosition = mul(float4(Input.p, 1.f), World);
    Output.position = mul(WorldPosition, VP);
    Output.worldPosition = WorldPosition.xyz;
    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    float3 DecalPosition = mul(float4(Input.worldPosition, 1.f), WorldToDecal).xyz;
    
    if (any(abs(DecalPosition) > 1.f))
    {
        discard;
    }
    
    float2 UV = float2(DecalPosition.y * 0.5f + 0.5f, 0.5f - DecalPosition.z * 0.5f);
    return g_txDecal.Sample(g_Sample, UV);
}
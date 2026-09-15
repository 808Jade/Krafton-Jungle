cbuffer constants : register(b0)
{
    matrix World;
    matrix ViewProjection;
    float4 CameraPos;
    float4 Scale;
};

struct VS_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PS_INPUT mainVS2(VS_INPUT input)
{   // 원근감 보정 안됨. 멀어지면 점점 얇아짐
    PS_INPUT output;
    float3 localPos = input.position;
    
    float3 dir = sign(localPos);
    
    float thickness = 0.025f;

    float3 adjustThickness = dir * (thickness / Scale.xyz);
    float3 outlinePos = localPos + adjustThickness;
    
    output.position = mul(float4(outlinePos, 1.0f), mul(World, ViewProjection));
    output.color = float4(1.0f, 1.0f, 0.0f, 1.0f);
    return output;
}

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    float3 localPos = input.position;
    
    float3 worldPos = mul(float4(localPos, 1.0f), World).xyz;
    float3 dist = distance(CameraPos.xyz, worldPos);
    
    float baseTickness = 1.0f;
    float distanceFactor = dist * 0.005f;
    float finalThickness = baseTickness * distanceFactor;
    
    float3 dir = sign(localPos);
    
    float3 adjustThickness = dir * (finalThickness / Scale.xyz);
    float3 outlinePos = localPos + adjustThickness;
    
    output.position = mul(float4(outlinePos, 1.0f), mul(World, ViewProjection));
    output.color = float4(1.0f, 1.0f, 0.0f, 1.0f);
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    return input.color;
}
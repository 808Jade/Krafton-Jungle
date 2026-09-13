// Todo: subuv
// 가장 단순한 SubUV Shader
// CurrentFrame으로 Atlas의 한 칸을 선택합니다.

cbuffer constants : register(b0)
{
    matrix MVP;
};

// Todo: subuv
cbuffer subuv : register(b1)
{
    // x = 현재 프레임
    // y = Atlas Column 수
    // z = Atlas Row 수
    float4 SubUVData;
};

// Todo: subuv
Texture2D AtlasTexture : register(t0);
SamplerState AtlasSampler : register(s0);

struct VS_INPUT
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    output.position = mul(float4(input.position, 1.0f), MVP);
    output.uv = input.uv;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float frame = SubUVData.x;
    float columnSize = max(SubUVData.y, 1.0f);
    float rowSize = max(SubUVData.z, 1.0f);

    // Frame 번호를 Atlas의 (Column, Row)로 변환
    float column = fmod(frame, columnSize);
    float row = floor(frame / columnSize);

    // 0~1 UV를 현재 Cell의 UV 영역으로 변환
    float2 cellSize = float2(1.0f / columnSize, 1.0f / rowSize);
    float2 atlasUV = input.uv * cellSize;
    atlasUV += float2(column, row) * cellSize;

    return AtlasTexture.Sample(AtlasSampler, atlasUV);
}

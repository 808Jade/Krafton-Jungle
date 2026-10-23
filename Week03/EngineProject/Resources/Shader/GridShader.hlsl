cbuffer GridCB : register(b0)
{
    float4x4 ViewProj;
    float3 CameraPos;
    int CellSize;
    float SubCellSize;
    int GridPlaneType;
    float2 Padding;
};

static const float GridSize = 400.0f; 
static const float4 Positions[4] =
{
    float4(-0.5, 0.5, 0.0, 1.0),
    float4(0.5, 0.5, 0.0, 1.0),
    float4(-0.5, -0.5, 0.0, 1.0),
    float4(0.5, -0.5, 0.0, 1.0)
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 coords : TEXCOORD0;
    float3 camPos : TEXCOORD1;
};

PS_INPUT mainVS(uint vid : SV_VertexID)
{
    PS_INPUT o;
    float4 localPos = Positions[vid];
    float4 worldPos = float4(0, 0, 0, 1);

    if (GridPlaneType == 0)
    {
        worldPos.xyz = localPos.xyz * GridSize;
        worldPos.xy += CameraPos.xy;
        o.coords = worldPos.xy;
    }
    else
    {
        float thickness = CellSize;
        float height = CellSize * 100;

        float2 camDir = normalize(CameraPos.xy);
        float2 rightDir = float2(-camDir.y, camDir.x);

        // 카메라를 향하는 세로 평면:
        // 가로 = rightDir 방향, 세로 = Z 방향
        float horizontalOffset = localPos.x * thickness;

        worldPos.xy = rightDir * horizontalOffset;
        worldPos.z = localPos.y * height;

        // x: Z축에서의 좌우 거리
        // y: 월드 Z 높이
        o.coords = float2(horizontalOffset, worldPos.z);
    }

    o.pos = mul(worldPos, ViewProj);
    o.camPos = CameraPos;
    return o;
}

#define MOD(x,y) ((x) - (y) * floor((x)/(y))) 

static const float4 CellColor = float4(0.9, 0.9, 0.9, 0.6);
static const float4 SubCellColor = float4(0.7, 0.7, 0.7, 0.35);
static const float4 AxisXColor = float4(1.0, 0.0, 0.0, 0.9);
static const float4 AxisYColor = float4(0.0, 1.0, 0.0, 0.9);
static const float4 AxisZColor = float4(0.0, 0.0, 1.0, 0.9);

static const float HeightToFadeRatio = 25.0f;
static const float MinFadeDistance = 5.0f;
static const float MaxFadeDistance = 50.0f;

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float fadeDist = clamp(abs(input.camPos.z) * HeightToFadeRatio, MinFadeDistance, MaxFadeDistance);

    if (GridPlaneType == 1)
    {
        float dx = max(fwidth(input.coords.x), 1e-6);
        float pixelDistZ = abs(input.coords.x) / dx;
        float alphaAxisZ = 1.0 - min(pixelDistZ / 2.0, 1.0);
        
        float distToCameraZ = length(input.camPos.xy);
        float falloffZ = smoothstep(1.0, 0.7, distToCameraZ / fadeDist);
        
        float4 color = float4(0.0, 0.0, 1.0, 0.0);
        
        color = lerp(color, AxisZColor, alphaAxisZ);
        
        color.a *= falloffZ;
        
        if (color.a < 0.01)
            discard;
            
        return color;
    }
    // 화면 1픽셀당 월드 좌표의 변화량
    float2 derivative = fwidth(input.coords);

    // 값 / (해당 픽셀의 변화량)을 통해 화면 픽셀(Screen Space) 단위의 거리를 구합니다.
    float2 subGrid = abs(frac(input.coords / SubCellSize - 0.5) - 0.5) * SubCellSize / derivative;
    float subLineDist = min(subGrid.x, subGrid.y);
    float alphaSub = 1.0 - min(subLineDist, 1.0); // 두께 1픽셀
    
    float2 mainGrid = abs(frac(input.coords / CellSize - 0.5) - 0.5) * CellSize / derivative;
    float mainLineDist = min(mainGrid.x, mainGrid.y);
    float alphaCell = 1.0 - min(mainLineDist / 1.5, 1.0); // 두께 1.5픽셀

    float2 axisGrid = abs(input.coords) / derivative;
    float alphaAxisX = 1.0 - min(axisGrid.y / 2.0, 1.0); // X축 두께 2픽셀
    float alphaAxisY = 1.0 - min(axisGrid.x / 2.0, 1.0); // Y축 두께 2픽셀

    float4 color = float4(SubCellColor.rgb, 0.0);
    
    color = lerp(color, SubCellColor, alphaSub);
    color = lerp(color, CellColor, alphaCell);
    color = lerp(color, AxisXColor, alphaAxisX);
    color = lerp(color, AxisYColor, alphaAxisY);

    float distToCamera = length(input.coords - input.camPos.xy);
    float falloff = smoothstep(1.0, 0.5, distToCamera / fadeDist);

    color.a *= falloff;

    if (color.a < 0.01)
        discard;
        
    return color;
}
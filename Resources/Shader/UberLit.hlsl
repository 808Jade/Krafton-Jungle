#pragma pack_matrix(row_major)

// EViewMode 순서와 일치해야 한다.
#define VIEWMODE_LIT          0
#define VIEWMODE_UNLIT        1
#define VIEWMODE_WORLDNORMAL  3

// ELightingModel 순서와 일치해야 한다.
#define LIGHTING_MODEL_LAMBERT      0
#define LIGHTING_MODEL_PHONG        1
#define LIGHTING_MODEL_BLINNPHONG   2

// EShadingModel 순서와 일치해야 한다.
#define SHADING_MODEL_GOURAUD  0
#define SHADING_MODEL_PHONG    1

// 기본값: Lit, Blinn-Phong, Phong
#ifndef VIEWMODE
#define VIEWMODE VIEWMODE_LIT
#endif

#ifndef LIGHTING_MODEL
#define LIGHTING_MODEL LIGHTING_MODEL_BLINNPHONG
#endif

#ifndef SHADING_MODEL
#define SHADING_MODEL SHADING_MODEL_PHONG
#endif

#ifndef HAS_NORMAL_MAP
#define HAS_NORMAL_MAP 0
#endif
#define USE_NORMAL_MAP (SHADING_MODEL == SHADING_MODEL_PHONG && HAS_NORMAL_MAP)

#define GOURAUD (VIEWMODE == VIEWMODE_LIT && SHADING_MODEL == SHADING_MODEL_GOURAUD)

struct FAmbientLightInfo
{
    float3 Color;
    float Padding;
};

struct FDirectionalLightInfo
{
    float3 Color;
    float Padding0;
    float3 Direction;
    int ShadowIndex;
    //float Padding1;
};

struct FPointLightInfo
{
    float3 Color;
    float AttenuationRadius;
    float3 Position;
    int ShadowIndex;
    //float Padding;
};

struct FSpotLightInfo
{
    float3 Color;
    float AttenuationRadius;
    float3 Position;
    float CosInnerCone;
    float3 Direction;
    float CosOuterCone;
    int ShadowIndex;
    float3 Padding_s;
};

struct FShadowInfo
{
    matrix LightViewProjection;
    uint AtlasIndex;
    uint AtlasStartX;
    uint AtlasStartY;
    uint AtlasSize;
    
    float Bias;
    float NormalBias;
    float Near;
    float Far;
    
    float LightSize;
    float3 Padding;
};

struct FVSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Tangent : TANGENT;
    float4 Color : COLOR; // 지금은 쓰지 않지만 나중에 쓰게 될 수 있음
    float2 UV : TEXCOORD;
};

struct FPSInput
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
    float3 WorldPosition : TEXCOORD1;
#if USE_NORMAL_MAP
    float3 Tangent : TANGENT;
    int BitangentSign : BITANGENT_SIGN;
#endif
#if GOURAUD
    float3 DiffuseLight : TEXCOORD2;
    float3 SpecularLight : TEXCOORD3;
#endif
};

cbuffer ViewConstants : register(b0)
{
    matrix ViewProjection;
    float3 CameraPosition;
    float ViewPadding;
};

cbuffer StaticMeshMaterialParams : register(b1)
{
    float4 BaseColor;
    float2 UVScrollOffset;
    uint bOpaque;
    float Shininess;
    float3 SpecularColor;
    uint bFlipNormalGreen;
};

cbuffer PerObjectConstants : register(b2)
{
    matrix World;
    matrix NormalMatrix; // World의 역전치
};

cbuffer LightingConstants : register(b3)
{
    FAmbientLightInfo AmbientLight;
    FDirectionalLightInfo DirectionalLight;
};

cbuffer TileLightConstants : register(b4)
{
    matrix TileInverseViewProjection;
    uint TileScreenWidth;
    uint TileScreenHeight;
    uint TileCountX;
    uint TileCountY;
    uint StructuredPointLightCount;
    uint StructuredSpotLightCount;
    uint PointTileStride;
    uint SpotTileStride;
    uint bVisualizeLightCount;
    uint3 TileCullingPadding;
};

cbuffer DirectionalShadowConstants : register(b5)
{
    matrix CascadeViewProjection[4];
    float4 CascadeSplits;
    float4 CascadeOffsets[4];
    float4 CascadeBiases;
    int NumCascades;
    float NormalBias;
    float2 CSMPadding;
};

StructuredBuffer<FPointLightInfo> StructuredPointLights : register(t8);
StructuredBuffer<FSpotLightInfo> StructuredSpotLights : register(t9);
StructuredBuffer<uint> PointTileList : register(t10);
StructuredBuffer<uint> SpotTileList : register(t11);
StructuredBuffer<FShadowInfo> StructuredShadowData : register(t12);

Texture2D AlbedoTexture : register(t0);

#if USE_NORMAL_MAP
Texture2D NormalTexture : register(t1);
#endif

Texture2DArray<float> ShadowTextureArray : register(t2);
Texture2D<float> DirectionalLightShadowmapAtlas : register(t3);

SamplerState AlbedoSampler : register(s0);
SamplerState ShadowPointSampler : register(s1);
SamplerComparisonState ShadowCmpSampler : register(s2);

float3 DiffuseTerm(float3 Il, float3 N, float3 L)
{
    return Il * max(dot(N, L), 0.0f);
}

// Phong: A(R), B(V) Blinn-Phong: A(N), B(H)
float3 SpecularTerm(float3 Il, float3 A, float3 B, float n)
{
    return Il * pow(max(dot(A, B), 0.0f), n);
}

float ComputeDistanceAttenuation(float Distance, float Radius)
{
    return pow(saturate(1.0f - pow(Distance / Radius, 4)), 2);
}

void AccumulateLight(float3 Il, float3 N, float3 L, float3 V, float n, inout float3 DiffuseLight, inout float3 SpecularLight)
{
    float NdotL = max(dot(N, L), 0.0f);
    DiffuseLight += Il * NdotL; // Lambert
    if (NdotL > 0.0f)
    {
#if LIGHTING_MODEL == LIGHTING_MODEL_PHONG
        float3 R = 2.0f * dot(N, L) * N - L;
        // ★ NdotL을 곱해 빛이 스치는 어두운 곳에서는 스펙큘러가 0으로 부드럽게 사라지게 함
        SpecularLight += SpecularTerm(Il, R, V, n) * NdotL;
#elif LIGHTING_MODEL == LIGHTING_MODEL_BLINNPHONG
        float3 H = normalize(L + V);
        SpecularLight += SpecularTerm(Il, N, H, n) * NdotL;
#endif
    }
}

float3 GetLightCountColor(uint LightCount)
{
    if (LightCount == 0u)
        return float3(0.0f, 0.0f, 0.0f);
    if (LightCount <= 4u)
        return float3(0.0f, 0.0f, 1.0f);
    if (LightCount <= 16u)
        return float3(0.0f, 1.0f, 0.0f);
    if (LightCount <= 32u)
        return float3(1.0f, 1.0f, 0.0f);
    if (LightCount <= 64u)
        return float3(1.0f, 0.5f, 0.0f);
    return float3(1.0f, 0.0f, 0.0f);
}

// 일단 GOURAUD 전용
void ComputeLighting(float3 N, float3 WorldPosition, float3 V, float n, out float3 DiffuseLight, out float3 SpecularLight)
{
    DiffuseLight = 0.0f;
    SpecularLight = 0.0f;

    // Directional Light
    {
        float3 Il = DirectionalLight.Color;
        float3 L = -DirectionalLight.Direction;
        float GeoFactor = 1.0f;
        AccumulateLight(Il * GeoFactor, N, L, V, n, DiffuseLight, SpecularLight);
    }
}

// 라이트에서 픽셀 방향에서 절댓값이 가장 큰 축이 그 픽셀이 들어 있는 면이다
uint SelectCubeFace(float3 LightToPixel)
{
    float3 A = abs(LightToPixel);
    if (A.x >= A.y && A.x >= A.z)
         // Forward, Back
        return LightToPixel.x > 0 ? 0 : 1;
    if (A.y >= A.z)
        // Right, Left
        return LightToPixel.y > 0 ? 2 : 3;
    // Up, Down
    return LightToPixel.z > 0 ? 4 : 5;
}

//비선형인(NDC.z인) ShadowMap 샘플링 뎁스값을 선형으로 만듭니다.
float LinearizeShadowDepth(float depth, float Near, float Far)
{
    //Projection의 ZScale = F / F-N
    float ZScale = Far / (Far - Near);
    
    //ZOffset = -NF / F - N
    float ZOffset = (-Near * Far) / (Far - Near);
    
    //            [0      0       ZScale  1]
    //[x y z 1] * [XScale 0       0       0]
    //            [0      YScale  0       0]
    //            [0      0       ZOffset 0]
    
    //depth(NDC.z) = Clip.z / Clip.w
    //              = (ZScale * x + ZOffset) / x
    //x = ZOffset / (depth - ZScale)
    return ZOffset / (depth - ZScale);
}

float PCF_CmpLinear(float2 UV, float ShadowIndex, float Ref)
{
    //비교 샘플러 사용
    float StoredDepth = ShadowTextureArray.SampleCmpLevelZero(ShadowCmpSampler, float3(UV, ShadowIndex), Ref);
    return StoredDepth;
}

float PCF_Software3x3(float2 TexelSize, float2 UV, float ShadowIndex, float Ref)
{
    //3x3 Software PCF
    //해상도 512로 일단 하드코딩
    float Lit;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            float Stored = ShadowTextureArray.SampleLevel(ShadowPointSampler, float3(UV + float2(x, y) * TexelSize, ShadowIndex), 0);
            Lit += (Ref) > Stored ? 0.0 : 1.0;
        }
    }
    Lit /= 9.0;
    return Lit;
}

float PCF_Software5x5(float2 TexelSize, float2 UV, float ShadowIndex, float Ref)
{
    //5x5 Software PCF
    float Lit = 0;
    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            float Stored = ShadowTextureArray.SampleLevel(ShadowPointSampler, float3(UV + float2(x, y) * TexelSize, ShadowIndex), 0);
            Lit += (Ref) > Stored ? 0.0 : 1.0;
        }
    }
    Lit /= 25.0;
    return Lit;
}

float PCF_CmpLinear_Software3x3(float2 TexelSize, float2 UV, float ShadowIndex, float Ref)
{
    //Comparison Linear + 3x3 Software PCF
    float Lit = 0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            float Stored = ShadowTextureArray.SampleCmpLevelZero(ShadowCmpSampler, float3(UV + float2(x, y) * TexelSize, ShadowIndex), Ref);
            Lit += Stored;
        }
    }
    Lit /= 9.0;
    return Lit;
}

//[입력] UV, Slice(=ShadowIndex + 면), ReceiverNdcZ, Info

//0) 선형화
//   dR = Near·Far / (Far − ReceiverNdcZ·(Far − Near))

//1) Blocker Search
//   SearchRadiusUV = LightSizeUV · (dR − Near) / dR
//   탐색 원 안의 N개 탭(Poisson)에서 섀도맵 깊이 z를 Point 샘플러로 읽어
//     z < ReceiverNdcZ − Bias 이면 Blocker → 선형화해서 합산, 개수++
//   개수 == 0 → return 1 (완전히 밝음)
//   dB = 합 / 개수

//2) Penumbra
//   PenumbraRatio = (dR − dB) / dB                       ← 닮은 삼각형
//   FilterRadiusUV = PenumbraRatio · LightSizeUV · Near / dR
//   FilterRadiusUV = clamp(FilterRadiusUV, TexelSize, MaxRadiusUV)

//3) PCF
//   같은 Poisson 탭을 FilterRadiusUV만큼 벌려
//     SampleCmpLevelZero(ShadowCmpSampler, float3(UV + Offset·Radius, Slice), ReceiverNdcZ − Bias)
//   평균 → 0~1

float PCSS(float2 TexelSize, float2 UV, float ShadowIndex, float ReceiverNdcZ, FShadowInfo Info)
{
    const float Ref = ReceiverNdcZ - Info.Bias;
    
    //Receive Depth를 선형 보간
    
    float dR = LinearizeShadowDepth(ReceiverNdcZ, Info.Near, Info.Far);
    
    //주변 픽셀의 깊이 중 SampleDepth < Receiver Depth 인 깊이만 평균내어 dB를 구한다.
    float dB = 0.0;
    float BlockerCount = 0;
    
    //얼마나 주변 픽셀로 적용해야 하지?
    //dB 평균 구하기.
    //Near 위치에서의 절두체 길이
    float SearchWorld = Info.LightSize * (dR - Info.Near) / dR;
    //world 단위를 uv 단위로 바꾸기 위해
    float SearchUV = SearchWorld / (2.0 * Info.Near * tan(90.0 / 2.0 * 3.14159 / 180.0));
    
    //일단 32개 텍셀보다는 작도록 설정
    SearchUV = min(SearchUV, TexelSize * 16.0);
    float SearchStep = SearchUV / 2.0;
    
    //실제 SearchUV / TexelSize만큼 진행하면 너무 샘플링 수가 많아서 스텝을 늘리는 방식으로 찾기
    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            //Blocker의 깊이를 가져와서 dB를 구해야 하기에 Point Sampler로 한다.
            //비교 샘플러로 하면 깊이 값이 보간되니까 안된다.
            float BlockerDepth = ShadowTextureArray.SampleLevel(ShadowPointSampler, float3(UV + float2(x, y) * SearchStep, ShadowIndex), 0);
        
            //샘플링한 깊이가 더 작은 Blocker인 깊이만 합산한다.
            if (BlockerDepth < Ref)
            {
                dB += LinearizeShadowDepth(BlockerDepth, Info.Near, Info.Far);
                BlockerCount += 1.0;
            }
        }
    }
        
    //주변 픽셀 중에 Blocker가 없다면 그림자 없음 처리
    if (BlockerCount == 0)
    {
        return 1.0;
    }
    
    //평균내기
    dB /= BlockerCount;
    
    //P = W * (dR - dB) / dB
    //W : 광원 크기
    //dR : 광원부터 Receiver까지
    //dB : 광원부터 Blocker까지
    
    //폭 반경
    //월드 단위이다.
    float P = Info.LightSize * (dR - dB) / dB;
    
    //반그림자 폭을 UV 반경으로 바꾼다.
    //반그림자의 절반 폭 = 커널 반경이다.
    //UV는 0 ~ 1이므로 dR 깊이 지점의 절두체 총 길이로 나눠서 Normalize하면 된다.
    //FOV 90도로 하드코딩
    float FilterRadiusUV = P / (2 * dR * tan(90.0 / 2.0 * 3.14159 / 180.0));
    //상한
    FilterRadiusUV = min(FilterRadiusUV, TexelSize * 16);
    
    //반그림자 폭의 절반이 커널의 반경이다.
    float Step = FilterRadiusUV / 2.0;
    
    Step = max(Step, TexelSize * 0.5f); // 너무 좁으면 같은 위치만 반복하므로 하한
    
    //방법 1. 샘플링하는 횟수를 늘린다. : 폭이 넓어지면 너무 많아짐
    //방법 2. 5x5로 크기는 고정. 샘플링하는 간격을 늘린다. : 품질이 1번보다는 안좋음.
    //방법 2를 쓰도록 함.
    float Lit = 0.0f;
    for (int y_2 = -2; y_2 <= 2; ++y_2)
    {
        for (int x_2 = -2; x_2 <= 2; ++x_2)
        {
            Lit += ShadowTextureArray.SampleCmpLevelZero(ShadowCmpSampler, float3(UV + float2(x_2, y_2) * Step, ShadowIndex), Ref);            
        }
        
    }
    
    return Lit / 25.0f;
}

// 반환값: 1 = 빛 받음, 0 = 그림자
float ComputeShadowFactor(float ShadowIndex, float3 WorldPosition)
{    
    //-1이면 그림자 없는 라이트
    if (ShadowIndex < 0)
        return 1.0f;

    FShadowInfo Info = StructuredShadowData[ShadowIndex];

    // light Clip Space 클립 공간
    float4 LightClip = mul(float4(WorldPosition, 1.0f), Info.LightViewProjection);
    // 광원 뒤쪽
    if (LightClip.w <= 0.0f)
        return 1.0f; 

    //원근 나눗셈은 PS에서 (VS에서 나누고 보간하면 원근 보정이 깨짐)
    float3 Ndc = LightClip.xyz / LightClip.w;

    //NDC → UV. D3D 텍스처는 v가 아래로 증가하므로 y를 뒤집는다
    float2 UV = float2(Ndc.x * 0.5f + 0.5f, 1.0f - (Ndc.y * 0.5f + 0.5f));

    //범위 검사: 섀도맵 밖이거나 Far 너머면 그림자 판정 불가 : 빛 받음
    //any 함수는 입력된 값 중 하나라도 0이 아닌 것(True)이 있으면 true를 반환
    //들어온 UV 중 u와 v중 하나라도 0.0보다 작다면으로 해석됨
    if (any(UV < 0.0f) || any(UV > 1.0f) || Ndc.z < 0.0f || Ndc.z > 1.0f)
        return 1.0f;
    
    // 아틀라스 UV 구하는 로직
    float AtlasWidth, AtlasHeight, NumPages;
    ShadowTextureArray.GetDimensions(AtlasWidth, AtlasHeight, NumPages);
    
    float2 AtlasUV = (float2(Info.AtlasStartX, Info.AtlasStartY) + UV * Info.AtlasSize) / float2(AtlasWidth, AtlasHeight);
    
    //Point Sampler

    //float StoredDepth = ShadowTextureArray.SampleLevel(ShadowPointSampler, float3(UV, ShadowIndex), 0);
    //return (Ndc.z - Info.Bias) > StoredDepth ? 0.0f : 1.0f;

    // 비교 + 상수 Bias
    // 샘플링 한 것보다 크다면 그림자임
    
    //ComparisonLinearSampler

    //return PCF_CmpLinear(UV, ShadowIndex, Ndc.z - Info.Bias);
    
    //3x3 Software PCF
    //해상도 512로 일단 하드코딩
    //float2 TexelSize = 1.0 / 1024.0;
    //float Lit = 0;
    //return PCF_Software3x3(TexelSize, UV, ShadowIndex, Ndc.z - Info.Bias, Lit);
       
    
    //5x5 Software PCF
    //해상도 일단 하드코딩
    //float2 TexelSize = 1.0 / 512.0;
    //return PCF_Software5x5(TexelSize, UV, ShadowIndex, Ndc.z - Info.Bias);
    
    
    //Comparison Linear + 3x3 Software PCF
    
    ////해상도 하드코딩
    //float2 TexelSize = 1.0 / 1024.0;
    //return PCF_CmpLinear_Software3x3(TexelSize, UV, ShadowIndex, Ndc.z - Info.Bias);
    
    //return PCSS(1.0 / 1024.0, UV, ShadowIndex, Ndc.z, Info);
    
    //해상도 하드코딩
    float2 TexelSize = 1.0 / 1024.0;
    
    float2 TileMin = (float2(Info.AtlasStartX, Info.AtlasStartY) + 0.5f) * TexelSize;
    float2 TileMax = (float2(Info.AtlasStartX, Info.AtlasStartY) + Info.AtlasSize - 0.5f) * TexelSize;
        
    float Lit = 0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x < 1; ++x)
        {
            float2 SampleUV = clamp(AtlasUV + float2(x, y) * TexelSize, TileMin, TileMax);
            float Stored = ShadowTextureArray.SampleCmpLevelZero(ShadowCmpSampler, float3(AtlasUV, Info.AtlasIndex), Ndc.z - Info.Bias);
            Lit += Stored;
        }
    }
    Lit /= 9.0;
    return Lit;
}

// Directional Light 전용 그림자 판정 (CSM 아틀라스 텍스처 t3 및 DirectionalShadowConstants b5 사용)
float ComputeDirectionalShadowFactor(float3 WorldPosition, float3 Normal)
{
    if (NumCascades <= 0)
        return 1.0f;

    int CascadeIndex = 0;
    float3 L = -DirectionalLight.Direction;

    // 1. 빛과 표면 사이의 각도 계산 (비스듬할수록 오프셋을 더 많이 줌)
    float NdotL = dot(Normal, L);
    float NormalOffsetScale = clamp(1.0f - saturate(NdotL), 0.0f, 1.0f);

    // 2. 표면 노멀 방향으로 월드 좌표를 이동 (NormalBias: 상수 버퍼 b5에서 전달)
    float3 ShadowWorldPos = WorldPosition + Normal * (NormalBias * NormalOffsetScale);

    // 3. 이동된 좌표로 Light Clip Space 계산
    float4 LightClip = mul(float4(ShadowWorldPos, 1.0f), CascadeViewProjection[CascadeIndex]);
    if (LightClip.w <= 0.0f)
        return 1.0f;

    float3 Ndc = LightClip.xyz / LightClip.w;
    float2 BaseUV = float2(Ndc.x * 0.5f + 0.5f, 1.0f - (Ndc.y * 0.5f + 0.5f));

    if (any(BaseUV < 0.0f) || any(BaseUV > 1.0f) || Ndc.z < 0.0f || Ndc.z > 1.0f)
        return 1.0f;

    float4 Offset = CascadeOffsets[CascadeIndex];
    float2 AtlasUV = BaseUV * Offset.xy + Offset.zw;

    float StoredDepth = DirectionalLightShadowmapAtlas.SampleLevel(ShadowPointSampler, AtlasUV, 0);

    // 4. 깊이 비교 (Normal Offset이 Acne를 제거하므로 작은 Depth Bias 적용)
    float DepthBias = CascadeBiases[CascadeIndex];
    return (Ndc.z - DepthBias) > StoredDepth ? 0.0f : 1.0f;
}

void ComputeTiledLighting(float3 N,
#if USE_NORMAL_MAP
    float3 GeoNormal,
#endif
float3 WorldPosition, float3 V, float n, float2 PixelPosition, out float3 DiffuseLight, out float3 SpecularLight)
{
    DiffuseLight = 0.0f;
    SpecularLight = 0.0f;

    // 화면 전체에 영향을 주는 Directional Light는 타일 목록에 넣지 않음
    {
        float3 Il = DirectionalLight.Color;
        float3 L = -DirectionalLight.Direction;
#if USE_NORMAL_MAP
        float GeoFactor = saturate(dot(GeoNormal, L) * 4.0f);
        float3 ShadowNormal = GeoNormal;
#else
        float GeoFactor = 1.0f;
        float3 ShadowNormal = N;
#endif
        float Shadow = ComputeDirectionalShadowFactor(WorldPosition, ShadowNormal);
        
        AccumulateLight(Il * GeoFactor * Shadow, N, L, V, n, DiffuseLight, SpecularLight);
    }

    uint2 TileCoord = min(uint2(PixelPosition) / 16u, uint2(TileCountX - 1u, TileCountY - 1u));
    uint TileIndex = TileCoord.y * TileCountX + TileCoord.x;

    // Point Light
    uint PointListStart = TileIndex * PointTileStride;
    uint PointCount = min(PointTileList[PointListStart], 64u);
    for (uint ListIndex = 0; ListIndex < PointCount; ++ListIndex)
    {
        uint LightIndex = PointTileList[PointListStart + 1u + ListIndex];
        if (LightIndex >= StructuredPointLightCount)
            continue;

        FPointLightInfo Light = StructuredPointLights[LightIndex];
        float3 ToLight = Light.Position - WorldPosition;
        float Distance = length(ToLight);
        float Radius = Light.AttenuationRadius;
        if (Distance <= 0.0f || Distance >= Radius)
            continue;

        float3 L = ToLight / Distance;
#if USE_NORMAL_MAP
        float GeoFactor = saturate(dot(GeoNormal, L) * 4.0f);
        if (GeoFactor <= 0.0f)
            continue;
#else
        float GeoFactor = 1.0f;
#endif

        float DistanceAttenuation = ComputeDistanceAttenuation(Distance, Radius);
        
        //선택할 ShadowIndex
        uint PointShadowIndex = SelectCubeFace(-ToLight);
        //spot light는 ComputeShadowFactor()내부에서 검사로 충분하지만 PointLight는 PointShadowIndex를 더해버리면서
        //ShadowIndex -1이 양수가 될 수 있다. 그래서 여기서도 한번 검사해줌.
        float Shadow = Light.ShadowIndex < 0 ? 1.0 : ComputeShadowFactor(Light.ShadowIndex + PointShadowIndex, WorldPosition);
        
        float3 Il = Light.Color * DistanceAttenuation * GeoFactor * Shadow;
        AccumulateLight(Il, N, L, V, n, DiffuseLight, SpecularLight);
    }

    // Spot Light
    uint SpotListStart = TileIndex * SpotTileStride;
    uint SpotCount = min(SpotTileList[SpotListStart], 64u);
    for (uint SpotListIndex = 0; SpotListIndex < SpotCount; ++SpotListIndex)
    {
        uint LightIndex = SpotTileList[SpotListStart + 1u + SpotListIndex];
        if (LightIndex >= StructuredSpotLightCount)
            continue;

        FSpotLightInfo Light = StructuredSpotLights[LightIndex];
        float3 ToLight = Light.Position - WorldPosition;
        float Distance = length(ToLight);
        float Radius = Light.AttenuationRadius;
        if (Distance <= 0.0f || Distance >= Radius)
            continue;

        float3 L = ToLight / Distance;
#if USE_NORMAL_MAP
        float GeoFactor = saturate(dot(GeoNormal, L) * 4.0f);
        if (GeoFactor <= 0.0f)
            continue;
#else
        float GeoFactor = 1.0f;
#endif
        //그림자 판단
        float Shadow = ComputeShadowFactor(Light.ShadowIndex, WorldPosition);        
        
        float DistanceAttenuation = ComputeDistanceAttenuation(Distance, Radius);
        float CosAngle = dot(-L, normalize(Light.Direction));
        float SpotAttenuation = Light.CosInnerCone == Light.CosOuterCone
            ? (CosAngle > Light.CosOuterCone ? 1.0f : 0.0f)
            : saturate((CosAngle - Light.CosOuterCone) / (Light.CosInnerCone - Light.CosOuterCone));

        float3 Il = Light.Color * DistanceAttenuation * SpotAttenuation * GeoFactor * Shadow;
        AccumulateLight(Il, N, L, V, n, DiffuseLight, SpecularLight);
    }
}

FPSInput mainVS(FVSInput Input)
{
    FPSInput Output;

    float4 WorldPosition = mul(float4(Input.Position, 1.0f), World);

    Output.Position = mul(WorldPosition, ViewProjection);
    Output.WorldPosition = WorldPosition.xyz;
    Output.Normal = mul(Input.Normal, (float3x3) NormalMatrix);
#if USE_NORMAL_MAP
    Output.Tangent = mul(Input.Tangent.xyz, (float3x3) World);
    Output.BitangentSign = Input.Tangent.w;
#endif
    Output.UV = Input.UV;
#if GOURAUD
    float3 N = normalize(Output.Normal);
    float3 V = normalize(CameraPosition - Output.WorldPosition);
    // TODO
    ComputeLighting(N, Output.WorldPosition, V, Shininess, Output.DiffuseLight, Output.SpecularLight);
    
#endif
    return Output;
}

float4 mainPS(FPSInput Input) : SV_TARGET
{
    float4 FinalColor = float4(0.0f, 0.0f, 0.0f, 1.0f);

    float4 TextureColor = AlbedoTexture.Sample(AlbedoSampler, Input.UV + UVScrollOffset);
    float4 SurfaceColor = TextureColor * BaseColor;

    float3 Albedo = SurfaceColor.rgb;
    float Alpha = bOpaque ? 1.0f : SurfaceColor.a;

    float3 Kd = Albedo;
    float3 Ka = Kd; // 나중에 Ka 를 개별로 설정할 수 있음
    float3 Ks = SpecularColor;
    
    float3 Normal = normalize(Input.Normal);
    
#if USE_NORMAL_MAP
    // 그람 슈미트 직교화
    float3 Tangent = normalize(Input.Tangent - dot(Input.Tangent, Normal) * Normal);
    float3 BiTangent = cross(Normal, Tangent) * Input.BitangentSign;
    float3x3 TBN = float3x3(Tangent, BiTangent, Normal);
    // Texture Normal 매핑
    float3 NormalSample = NormalTexture.Sample(AlbedoSampler, Input.UV + UVScrollOffset).xyz;
    float3 TextureNormal = normalize(NormalSample * 2.0f - 1.0f);
    TextureNormal.y = bFlipNormalGreen ? -TextureNormal.y : TextureNormal.y;
    // TBN 행렬을 이용하여 월드 공간으로 변환
    float3 WorldNormal = mul(TextureNormal, TBN);
#else
    float3 WorldNormal = Normal;
#endif
   
#if VIEWMODE == VIEWMODE_UNLIT
    FinalColor = float4(Albedo, Alpha);
#elif VIEWMODE == VIEWMODE_WORLDNORMAL
    // 색이 아니라 데이터이므로 Gamma Encode Pass에서 인코딩하지 않는다.
    FinalColor = float4(normalize(WorldNormal) * 0.5f + 0.5f, 1.0f);
#else // Lit
    if (bVisualizeLightCount != 0u)
    {
        uint2 PixelCoord = uint2(Input.Position.xy);
        uint2 TileCoord = min(PixelCoord / 16u, uint2(TileCountX - 1u, TileCountY - 1u));
        uint TileIndex = TileCoord.y * TileCountX + TileCoord.x;
        
        uint PointListStart = TileIndex * PointTileStride;
        uint SpotListStart = TileIndex * SpotTileStride;
        
        uint PointCount = min(PointTileList[PointListStart], 64u);
        uint SpotCount = min(SpotTileList[SpotListStart], 64u);
        
        float3 Color = GetLightCountColor(PointCount + SpotCount);
        
        uint2 LocalCoord = PixelCoord % 16u;

        if (LocalCoord.x == 0u || LocalCoord.y == 0u)
        {
            Color *= 0.2f;
        }

        return float4(Color, 1.0f);
    }
    
    float3 DiffuseLight;
    float3 SpecularLight;
#if SHADING_MODEL == SHADING_MODEL_GOURAUD
    DiffuseLight = Input.DiffuseLight;
    SpecularLight = Input.SpecularLight;
#else // Phong
    float3 N = normalize(WorldNormal);
    float3 V = normalize(CameraPosition - Input.WorldPosition);
    
#if USE_NORMAL_MAP
    ComputeTiledLighting(N, Normal, Input.WorldPosition, V, Shininess, Input.Position.xy, DiffuseLight, SpecularLight);
#else
    ComputeTiledLighting(N, Input.WorldPosition, V, Shininess, Input.Position.xy, DiffuseLight, SpecularLight);
#endif
    
#endif
    FinalColor = float4(Ka * AmbientLight.Color + Kd * DiffuseLight + Ks * SpecularLight, Alpha);
#endif
    return FinalColor;
}

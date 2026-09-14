#include "EnginePCH.h"
#include "AssetManager.h"
#include "Render/Buffer.h"

#include "Render/Vertex.h"
#include "Render/Texture2D.h"

UAssetManager& UAssetManager::Get()
{
    static UAssetManager* Instance = FObjectFactory::ConstructObject<UAssetManager>();
    return *Instance;
}

void UAssetManager::Init(FRenderer* InRenderer)
{
    Renderer = InRenderer;

    if (!Renderer)
    {
        return;
    }

    // 메시 데이터 업로드
    FStaticMeshData CubeData = FGeometryGenerator::CreateCube(1.0f);
    MeshMap[FString("Cube")] = Renderer->CreateMesh(CubeData);

    FStaticMeshData ConeData = FGeometryGenerator::CreateCone(1.0f, 1.0f, 20, FVector4(1.0f, 0.0f, 0.0f, 1.0f));
    MeshMap[FString("Cone")] = Renderer->CreateMesh(ConeData);

    FStaticMeshData SphereData = FGeometryGenerator::CreateSphere(1.0f, 20, 10, FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    MeshMap[FString("Sphere")] = Renderer->CreateMesh(SphereData);

    FStaticMeshData PlaneData = FGeometryGenerator::CreatePlane(1.0f,FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    MeshMap[FString("Plane")] = Renderer->CreateMesh(PlaneData);
}

void UAssetManager::Shutdown()
{
    MeshMap.Reset();

    // Todo: subuv
    TextureMap.Reset();
}

void UAssetManager::SetRenderer(FRenderer* InRenderer)
{
    Renderer = InRenderer;
}

UStaticMesh* UAssetManager::GetMesh(FString InName)
{
    if (!Renderer)
    {
        return nullptr;
    }

    ///auto it = MeshMap.find(InName);
    auto* foundMeshOrNull = MeshMap.FindOrNull(InName);
    //if (it == MeshMap.end())
    if (foundMeshOrNull == nullptr)
    {
        // Todo: subuv, Move to other class
        if (InName == "ParticleQuad")
        {
            const FParticleVertex Vertices[] =
            {
                { FVector(-0.5f,  0.5f, 0.0f), FVector2(0.0f, 0.0f) },
                { FVector(0.5f,  0.5f, 0.0f), FVector2(1.0f, 0.0f) },
                { FVector(0.5f, -0.5f, 0.0f), FVector2(1.0f, 1.0f) },
                { FVector(-0.5f, -0.5f, 0.0f), FVector2(0.0f, 1.0f) }
            };

            const uint32 Indices[] =
            {
                0, 2, 1,
                0, 3, 2
            };

            TSharedPtr<FVertexBuffer> VB = Renderer->CreateVertexBuffer(
                Vertices,
                sizeof(Vertices),
                sizeof(FParticleVertex));

            TSharedPtr<FIndexBuffer> IB = Renderer->CreateIndexBuffer(
                Indices,
                ARRAYSIZE(Indices));

            MeshMap[InName] = Renderer->CreateMesh(VB, IB);

            return MeshMap[InName];
        }

        FStaticMeshData Data = FGeometryGenerator::GetMeshData(InName);
        TSharedPtr<FVertexBuffer> vb = Renderer->CreateVertexBuffer(Data.Vertices.GetData(), sizeof(FVertex) * (UINT)Data.Vertices.Num(), sizeof(FVertex));
        TSharedPtr<FIndexBuffer> ib = Renderer->CreateIndexBuffer(Data.Indices.GetData(), Data.Indices.Num());

        MeshMap[InName] = Renderer->CreateMesh(vb, ib);
    }

    return MeshMap[InName];
}

FShader* UAssetManager::GetShader(FString InName)
{
    if (!Renderer)
    {
        return nullptr;
    }

    //auto it = ShaderMap.find(InName);
    auto* foundShaderOrNull = ShaderMap.FindOrNull(InName);

    //if (it == ShaderMap.end())
    if (foundShaderOrNull == nullptr)
    {
        std::wstring wstr(InName.begin(), InName.end());

        // Todo: subuv
        if (InName == "Shader/ParticleSubUVShader.hlsl")
        {
            ShaderMap[InName] = Renderer->CreateShader(wstr.c_str(), FParticleVertex::GetLayout());
        }
        else
        {
            D3D11_INPUT_ELEMENT_DESC inputDesc[] =
            {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            };

            ShaderMap[InName] = Renderer->CreateShader(wstr.c_str(), inputDesc, 2);
        }

        //D3D11_INPUT_ELEMENT_DESC inputDesc[] =
        //{
        //    {"POSITION" , 0 , DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        //    {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        //};

        //ShaderMap[InName] = Renderer->CreateShader(wstr.c_str(), inputDesc, 2);
    }
    return ShaderMap[InName].get();
}

// Todo: subuv
UTexture2D* UAssetManager::GetTexture(const FString& InPath)
{
    assert(Renderer != nullptr);

    UTexture2D** FoundTextureOrNull = TextureMap.FindOrNull(InPath);
    if (FoundTextureOrNull != nullptr)
    {
        return *FoundTextureOrNull;
    }

    UTexture2D* Texture = Renderer->CreateShaderResource(InPath);
    assert(Texture != nullptr);
    
    TextureMap[InPath] = Texture;

    return Texture;
}

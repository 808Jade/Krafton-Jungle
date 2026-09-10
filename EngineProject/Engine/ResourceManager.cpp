#include "EnginePCH.h"
#include "ResourceManager.h"
#include "Render/Buffer.h"

FResourceManager& FResourceManager::GetInstance()
{   // 렌더러 생성되고 이후에
    static FResourceManager instance;
    
    return instance;
}

void FResourceManager::Init(FRenderer* InRenderer)
{
    Renderer = InRenderer;

    if (!Renderer)
    {
        return;
    }

    // 메시 데이터 업로드
    FMeshData CubeData = FGeometryGenerator::CreateCube(1.0f);
    MeshMap[FString("Cube")] = Renderer->CreateMesh(CubeData);

    FMeshData ConeData = FGeometryGenerator::CreateCone(1.0f, 1.0f, 20, FVector4(1.0f, 0.0f, 0.0f, 1.0f));
    MeshMap[FString("Cone")] = Renderer->CreateMesh(ConeData);

    FMeshData SphereData = FGeometryGenerator::CreateSphere(1.0f, 20, 10, FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    MeshMap[FString("Sphere")] = Renderer->CreateMesh(SphereData);

    FMeshData PlaneData = FGeometryGenerator::CreatePlane(1.0f,FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    MeshMap[FString("Plane")] = Renderer->CreateMesh(PlaneData);
}

void FResourceManager::SetRenderer(FRenderer* InRenderer)
{
    Renderer = InRenderer;
}

FMesh* FResourceManager::GetMesh(FString InName)
{
    if (!Renderer)
    {
        return nullptr;
    }

    auto it = MeshMap.find(InName);
    if (it == MeshMap.end())
    {
        FMeshData Data = FGeometryGenerator::GetMeshData(InName);
        TSharedPtr<FVertexBuffer> vb = Renderer->CreateVertexBuffer(Data.Vertices.data(), sizeof(FVertex) * (UINT)Data.Vertices.size(), sizeof(FVertex));
        TSharedPtr<FIndexBuffer> ib = Renderer->CreateIndexBuffer(Data.Indices.data(), Data.Indices.size());

        VertexBufferMap[InName] = vb;
        indexBufferMap[InName] = ib;

        TSharedPtr<FMesh> mesh = Renderer->CreateMesh(vb, ib);
        MeshMap[InName] = mesh;
    }
    return MeshMap[InName].get();
}

FShader* FResourceManager::GetShader(FString InName)
{
    if (!Renderer)
    {
        return nullptr;
    }

    auto it = ShaderMap.find(InName);
    if (it == ShaderMap.end())
    {
        std::wstring wstr(InName.begin(), InName.end());

        D3D11_INPUT_ELEMENT_DESC inputDesc[] =
        {
            {"POSITION" , 0 , DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

        ShaderMap[InName] = Renderer->CreateShader(wstr.c_str(), inputDesc, 2);
    }
    return ShaderMap[InName].get();
}

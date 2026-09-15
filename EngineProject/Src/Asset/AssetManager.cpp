#include "EnginePCH.h"
#include "AssetManager.h"
#include "Render/Buffer.h"

#include "Render/Vertex.h"
#include "Render/Texture2D.h"
#include "Render/RenderCommand.h"


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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
	FStaticMeshData* CubeData = FGeometryGenerator::GetMeshData("Cube");
	MeshMap[FString("Cube")] = Renderer->CreateMesh(*CubeData);

	FStaticMeshData* ConeData = FGeometryGenerator::GetMeshData("Cone");
	MeshMap[FString("Cone")] = Renderer->CreateMesh(*ConeData);

	FStaticMeshData* SphereData = FGeometryGenerator::GetMeshData("Sphere");
	MeshMap[FString("Sphere")] = Renderer->CreateMesh(*SphereData);

	FStaticMeshData* PlaneData = FGeometryGenerator::GetMeshData("Plane");
	MeshMap[FString("Plane")] = Renderer->CreateMesh(*PlaneData);
}

void UAssetManager::Shutdown()
{
	MeshMap.Reset();

	// Todo: subuv
	TextureMap.Reset();
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

			TSharedPtr<FVertexBuffer> VB = RenderCommand::CreateStaticVertexBuffer(
				Vertices,
				sizeof(Vertices),
				sizeof(FParticleVertex));
			FStaticMeshData* Data = FGeometryGenerator::GetMeshData(InName);

			TSharedPtr<FIndexBuffer> IB = RenderCommand::CreateStaticIndexBuffer(
				Indices,
				ARRAYSIZE(Indices));

			MeshMap[InName] = Renderer->CreateMesh(VB, IB);

			return MeshMap[InName];
		}

		FStaticMeshData* Data = FGeometryGenerator::GetMeshData(InName);
		TSharedPtr<FVertexBuffer> vb = RenderCommand::CreateStaticVertexBuffer(Data->Vertices.GetData(), sizeof(FVertex) * (UINT)Data->Vertices.Num(), sizeof(FVertex));
		TSharedPtr<FIndexBuffer> ib = RenderCommand::CreateStaticIndexBuffer(Data->Indices.GetData(), Data->Indices.Num());

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
			ShaderMap[InName] = RenderCommand::CreateShader(wstr.c_str(), FParticleVertex::GetLayout());
		}
		else
		{
			D3D11_INPUT_ELEMENT_DESC inputDesc[] =
			{
				{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
				{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			};

			ShaderMap[InName] = RenderCommand::CreateShader(wstr.c_str(), inputDesc, 2);
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

UTexture2D* UAssetManager::LoadTexture(const FString& InPath)
{
	if (UTexture2D** Found = TextureMap.FindOrNull(InPath))
		return *Found;

	stbi_set_flip_vertically_on_load(false);
	int Width = 0, Height = 0, Channels = 0;
	unsigned char* Pixels = stbi_load(InPath.c_str(), &Width, &Height, &Channels, 4);
	if (!Pixels)
	{
		OutputDebugStringA(("[Texture] stbi_load failed: " + InPath + " (" + stbi_failure_reason() + ")\n").c_str());
		return nullptr;
	}

	D3D11_TEXTURE2D_DESC Desc{};
	Desc.Width = Width;
	Desc.Height = Height;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	TUniquePtr<FTexture2D> Resource = RenderCommand::CreateTexture2D(Desc, Pixels);

	if (!Resource) return nullptr;

	UTexture2D* Asset = FObjectFactory::ConstructObject<UTexture2D>();
	Asset->SetResource(std::move(Resource));
	Asset->SetPath(InPath);

	PathMap.Add(InPath, Asset);
	return Asset;
}

UFontAtlas* UAssetManager::LoadFontAtlas(const FString& JsonPath, const FString& AtlasTexturePath)
{
	UFontAtlas* Atlas = FObjectFactory::ConstructObject<UFontAtlas>();

	if (!Atlas->LoadFontAtlasJson(JsonPath))
	{
		return nullptr;
	}

	Atlas->AtlasTexture = LoadTexture(AtlasTexturePath);
	if (!Atlas->AtlasTexture)
	{
		return nullptr;
	}

	return Atlas;
}

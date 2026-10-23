#include "EnginePCH.h"
#include "AssetManager.h"
#include "Render/Buffer.h"

#include "Render/Vertex.h"
#include "Render/Texture2D.h"
#include "Render/RenderCommand.h"

#include "Render/RenderResourceManager.h"


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace
{
	UStaticMesh* CreateStaticMesh(const FStaticMeshData* MeshData)
	{
		UStaticMesh* Mesh = FObjectFactory::ConstructObject<UStaticMesh>();
		Mesh->VertexBuffer = RenderCommand::CreateStaticVertexBuffer(MeshData->Vertices.GetData(), sizeof(FVertex) * static_cast<uint32>(MeshData->Vertices.size()), sizeof(FVertex));
		Mesh->IndexBuffer = RenderCommand::CreateStaticIndexBuffer(MeshData->Indices.GetData(), static_cast<uint32>(MeshData->Indices.size()));
		return Mesh;
	}

}

UAssetManager& UAssetManager::Get()
{
	static UAssetManager* Instance = FObjectFactory::ConstructObject<UAssetManager>();
	return *Instance;
}

void UAssetManager::ScanAssets(const fs::path& AssetRoot)
{
	if (!fs::exists(AssetRoot))
	{
		LOG(Error, "Asset root not found: {}", AssetRoot.generic_string());
		return;
	}

	for (const fs::directory_entry& Entry : fs::recursive_directory_iterator(AssetRoot))
	{
		if (!Entry.is_regular_file()) continue;

		FString Key = fs::relative(Entry.path(), AssetRoot).generic_string();
		FString Path = Entry.path().generic_string();
		AssetPathMap.Add(Key, Path);
		LoadAsset(Key, Path);
	}
}

void UAssetManager::LoadAsset(const FString& Key, const FString& Path)
{
	FString Extension = fs::path(Path).extension().string();
	if (Extension == ".png")
	{
		if (UTexture2D* Texture = LoadTexture(Path))
		{
			AssetMap[Key] = Texture;
		}
	}
	if (Extension == ".json")
	{
		FString TexturePath = fs::path(Path).replace_extension(".png").string();
		if (UFont* Font = LoadFontAtlas(Path, TexturePath))
		{
			AssetMap[Key] = Font;
		}
	}
}

void UAssetManager::Init()
{
	ScanAssets("Assets");

	// 머티리얼이 참조하므로 반드시 먼저 만든다
	CreateDefaultTextures();

	CreateDefaultMeshes();
	CreateDefaultMaterials();
}

void UAssetManager::CreateDefaultTextures()
{
	// 텍스처가 지정되지 않은 머티리얼이 검게 나오지 않도록 하는 1x1 흰색 텍스처.
	// 셰이더에서 곱해도 결과가 변하지 않으므로 "텍스처 없음"의 기본값으로 쓴다.
	const uint32 WhitePixel = 0xFFFFFFFF;

	D3D11_TEXTURE2D_DESC Desc{};
	Desc.Width = 1;
	Desc.Height = 1;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	TUniquePtr<FTexture2D> Resource = RenderCommand::CreateTexture2D(Desc, &WhitePixel);
	if (!Resource)
	{
		return;
	}

	UTexture2D* WhiteTexture = FObjectFactory::ConstructObject<UTexture2D>();
	WhiteTexture->SetResource(std::move(Resource));
	WhiteTexture->SetPath("WhiteTexture");

	AssetMap["WhiteTexture"] = WhiteTexture;
}

void UAssetManager::CreateDefaultMeshes()
{
	// 메시 데이터 업로드
	FStaticMeshData* CubeData = FGeometryGenerator::GetMeshData("Cube");
	AssetMap["Cube"] = CreateStaticMesh(CubeData);

	FStaticMeshData* ConeData = FGeometryGenerator::GetMeshData("Cone");
	AssetMap["Cone"] = CreateStaticMesh(ConeData);

	FStaticMeshData* SphereData = FGeometryGenerator::GetMeshData("Sphere");
	AssetMap["Sphere"] = CreateStaticMesh(SphereData);

	FStaticMeshData* PlaneData = FGeometryGenerator::GetMeshData("Plane");
	AssetMap["Plane"] = CreateStaticMesh(PlaneData);

	const FParticleVertex Vertices[] =
	{
		{ FVector(0.0f, -0.5f,  0.5f), FVector2(0.0f, 0.0f) },
		{ FVector(0.0f, 0.5f,  0.5f), FVector2(1.0f, 0.0f) },
		{ FVector(0.0f, 0.5f, -0.5f), FVector2(1.0f, 1.0f) },
		{ FVector(0.0f, -0.5f, -0.5f), FVector2(0.0f, 1.0f) }
	};

	const uint32 Indices[] =
	{
		0, 1, 2,
		0, 2, 3
	};

	UStaticMesh* Mesh = FObjectFactory::ConstructObject<UStaticMesh>();
	Mesh->VertexBuffer = RenderCommand::CreateStaticVertexBuffer(
		Vertices,
		sizeof(Vertices), sizeof(FParticleVertex));
	Mesh->IndexBuffer = RenderCommand::CreateStaticIndexBuffer(
		Indices,
		ARRAYSIZE(Indices));

	AssetMap["ParticleQuad"] = Mesh;
}

void UAssetManager::CreateDefaultMaterials()
{
	UMaterial* DefaultMat = FObjectFactory::ConstructObject<UMaterial>();
	DefaultMat->Shader = FRenderResourceManager::GetShader("Resources/Shader/DefaultShader.hlsl");
	DefaultMat->Textures.Add(GetAssetByPath<UTexture2D>("WhiteTexture"));

	AssetMap["DefaultMaterial"] = DefaultMat;

	UMaterial* ParticleMat = FObjectFactory::ConstructObject<UMaterial>();
	ParticleMat->Shader = FRenderResourceManager::GetShader("Resources/Shader/ParticleSubUVShader.hlsl");
	ParticleMat->ParamBuffer = RenderCommand::CreateConstantBuffer(256);
	ParticleMat->Textures.Add(GetAssetByPath<UTexture2D>("Assets/CloudAtlas.png"));
	ParticleMat->BlendState = EBlendState::AlphaBlend;
	ParticleMat->DepthStencilState = EDepthStencilState::ReadOnly;

	AssetMap["SubUVMaterial"] = ParticleMat;
}

void UAssetManager::Shutdown()
{
}


UTexture2D* UAssetManager::LoadTexture(const FString& InPath)
{
	if (URenderAsset** Found = AssetMap.FindOrNull(InPath))
	{
		return Cast<UTexture2D>(*Found);
	}

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

	AssetMap.Add(InPath, Asset);
	return Asset;
}

UFont* UAssetManager::LoadFontAtlas(const FString& JsonPath, const FString& AtlasTexturePath)
{
	UFont* Atlas = FObjectFactory::ConstructObject<UFont>();

	if (!Atlas->LoadFontAtlasJson(JsonPath))
	{
		return nullptr;
	}

	Atlas->AtlasTexture = LoadTexture(AtlasTexturePath);
	if (!Atlas->AtlasTexture)
	{
		return nullptr;
	}

	AssetMap.Add(JsonPath, Atlas);
	return Atlas;
}

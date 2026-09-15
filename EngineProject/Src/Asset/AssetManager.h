#pragma once

#include "ObjectSystem/Object.h"
#include "ObjectSystem/Property.h"
#include "Render/Renderer.h"

class UStaticMesh;
struct FShader;

class UAssetManager : public UObject
{
	DECLARE_CLASS(UAssetManager, UObject)
private:
	UAssetManager() = default;

	UAssetManager(const UAssetManager& src) = delete;
	UAssetManager& operator= (const UAssetManager& src) = delete;
public:
	static UAssetManager& Get();

	void Init(FRenderer* renderer);
	void Shutdown();

	template <typename T>
	static T* GetAsset()
	{

	}

	UStaticMesh* GetMesh(FString InName);
	UStaticMesh* GetMesh(EPrimitiveType Type); // 오버로드(수정 중)
	FShader* GetShader(FString InName);

	// Todo: subuv
	UTexture2D* LoadTexture(const FString& InPath);
	UFontAtlas* LoadFontAtlas(const FString& JsonPath, const FString& AtlasTexturePath);


private:
	TMap<FString, URenderAsset*> PathMap;

	TMap<FString, UStaticMesh*> MeshMap;	// KEY: FILE NAME OR 쉐입 첫글자 대문자
	TMap<FString, TSharedPtr<FShader>> ShaderMap;	// KEY: FILE NAME

	TMap<FString, TSharedPtr<FVertexBuffer>> VertexBufferMap;
	TMap<FString, TSharedPtr<FIndexBuffer>> indexBufferMap;

	// Todo: subuv
	TMap<FString, UTexture2D*> TextureMap;

	FRenderer* Renderer;
};
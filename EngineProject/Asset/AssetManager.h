#pragma once

#include "ObjectSystem/Object.h"
#include "ObjectSystem/Property.h"
#include "Render/Renderer.h"

struct UStaticMesh;
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
	
	void SetRenderer(FRenderer* InRenderer);
	
	UStaticMesh* GetMesh(FString InName);
	FShader* GetShader(FString InName);
private:
	TMap<FString, TSharedPtr<UStaticMesh>> MeshMap;	// KEY: FILE NAME OR 쉐입 첫글자 대문자
	TMap<FString, TSharedPtr<FShader>> ShaderMap;	// KEY: FILE NAME

	TMap<FString, TSharedPtr<FVertexBuffer>> VertexBufferMap;
	TMap<FString, TSharedPtr<FIndexBuffer>> indexBufferMap;

	FRenderer* Renderer;
};
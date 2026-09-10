#pragma once
#include "Core/Containers.h"
#include "ObjectSystem/Property.h"
#include "Render/Renderer.h"

struct FMesh;
struct FShader;

class FResourceManager
{
private:
	FResourceManager() = default;

	FResourceManager(const FResourceManager& src) = delete;
	FResourceManager& operator= (const FResourceManager& src) = delete;
public:
	static FResourceManager& GetInstance();

	void Init(FRenderer* renderer);
	
	void SetRenderer(FRenderer* InRenderer);
	
	FMesh* GetMesh(FString InName);
	FShader* GetShader(FString InName);
private:
	TMap<FString, TSharedPtr<FMesh>> MeshMap;	// KEY: FILE NAME OR 쉐입 첫글자 대문자
	TMap<FString, TSharedPtr<FShader>> ShaderMap;	// KEY: FILE NAME

	TMap<FString, TSharedPtr<FVertexBuffer>> VertexBufferMap;
	TMap<FString, TSharedPtr<FIndexBuffer>> indexBufferMap;

	FRenderer* Renderer;
};
#pragma once

#include "Shader.h"

class FRenderResourceManager
{
public:
	static FRenderResourceManager& Get()
	{
		static FRenderResourceManager* Instance = new FRenderResourceManager();
		return *Instance;
	}

	static void Shutdown();
	static FShader* GetShader(const FString& InPath);
private:
	TMap<FString, TUniquePtr<FShader>> ShaderMap;
};
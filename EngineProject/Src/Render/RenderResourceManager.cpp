#include "EnginePCH.h"
#include "RenderResourceManager.h"

#include "RenderCommand.h"

void FRenderResourceManager::Shutdown()
{
	Get().ShaderMap.Empty();
}

FShader* FRenderResourceManager::GetShader(const FString& InPath)
{
	//auto it = ShaderMap.find(InName);
	auto* foundShaderOrNull = Get().ShaderMap.FindOrNull(InPath);

	//if (it == ShaderMap.end())
	if (foundShaderOrNull == nullptr)
	{
		std::wstring wstr(InPath.begin(), InPath.end());

		// Todo: subuv
		if (InPath == "Resources/Shader/ParticleSubUVShader.hlsl")
		{
			Get().ShaderMap[InPath] = RenderCommand::CreateShader(wstr.c_str(), FParticleVertex::GetLayout());
		}
		else
		{
			Get().ShaderMap[InPath] = RenderCommand::CreateShader(wstr.c_str(), FVertex::GetLayout());
		}


	}

	return Get().ShaderMap[InPath].get();
}
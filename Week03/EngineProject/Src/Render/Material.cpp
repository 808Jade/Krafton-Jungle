#include "EnginePCH.h"

#include "RenderCommand.h"

#include "Material.h"
#include "ObjectSystem/ObjectFactory.h"

UMaterial* UMaterial::CreateInstance(const UMaterial* Source)
{
	if (Source == nullptr)
	{
		return nullptr;
	}

	UMaterial* Instance = FObjectFactory::ConstructObject<UMaterial>();

	Instance->Shader = Source->Shader;
	Instance->Textures = Source->Textures;
	Instance->BlendState = Source->BlendState;
	Instance->DepthStencilState = Source->DepthStencilState;

	// ParamBuffer는 TUniquePtr라 복사할 수 없다.
	// 원본이 갖고 있으면 같은 크기로 새로 만들어 준다. 내용은 매 프레임 갱신되므로 옮기지 않는다.
	if (Source->ParamBuffer)
	{
		Instance->ParamBuffer = RenderCommand::CreateConstantBuffer(Source->ParamBuffer->GetBufferSize());
	}

	Instance->bIsInstance = true;

	return Instance;
}

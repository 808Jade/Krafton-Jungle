#pragma once

#include "Asset/RenderAsset.h"
#include "Render/RenderStates.h"
#include "Render/Texture2D.h"

class UMaterial : public URenderAsset
{
	DECLARE_CLASS(UMaterial, URenderAsset)
public:
	UMaterial() = default;
	virtual ~UMaterial() override = default;

	FShader* Shader;
	TArray<UTexture2D*> Textures;
	TUniquePtr<FConstantBuffer> ParamBuffer;
	EBlendState BlendState = EBlendState::Opaque;
	EDepthStencilState DepthStencilState = EDepthStencilState::Default;

private:

};
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

	// AssetManager가 들고 있는 공유 에셋이면 false, 오브젝트별 복제본이면 true.
	// 공유 에셋을 직접 수정하면 그걸 쓰는 모든 오브젝트가 같이 바뀌므로
	// 편집 직전에 이 값을 보고 복제한다.
	bool bIsInstance = false;

	// Source의 내용을 복사한 편집용 복제본을 만든다
	static UMaterial* CreateInstance(const UMaterial* Source);

private:

};
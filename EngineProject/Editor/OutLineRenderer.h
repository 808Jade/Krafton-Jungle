#pragma once

#include "Render/Renderer.h"
#include "Outline.h"

struct FOutlineData
{
	FMatrix World;
	FMatrix ViewProj;
	FVector4 CameraPos;
	FVector4 Scale;
};

class FOutlineRenderer
{
public:

	void Init(FRenderer* InRenderer);
	void SetMesh(FMesh* InMesh);
	void OnRender(const FOutline& InOutline, const FMatrix& ViewProj, const FVector4& CameraPos);
	
private:
	FRenderer* Renderer;

	FShader* Shader;
	FMesh* Mesh;

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> RasterizerState;
	TSharedPtr<FConstantBuffer> ConstantBuffer;
};
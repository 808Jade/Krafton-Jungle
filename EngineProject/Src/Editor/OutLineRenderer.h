#pragma once

#include "Render/Renderer.h"
#include "Render/PipelineState.h"
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
	void SetMesh(UStaticMesh* InMesh);
	void OnRender(const FOutline& InOutline, const FMatrix& ViewProj, const FVector4& CameraPos);
	
private:
	FRenderer* Renderer;

	TUniquePtr<FShader> Shader;
	FPipelineState PipelineState;
	UStaticMesh* Mesh;

	ComPtr<ID3D11RasterizerState> RasterizerState;
	TUniquePtr<FConstantBuffer> ConstantBuffer;
};
#pragma once

#include "Render/Buffer.h"
#include "Render/PipelineState.h"

struct TextTransformData
{
	FMatrix World;
	FMatrix ViewProj;
};


struct MSDFData
{
	float ScreenPx;
	FVector Pad = FVector();
};

class FTextRenderer
{
public:
	void Init(FRenderer* InRenderer, uint32 InMaxCharacters = 64);
	void OnRender(const FString& Text, const FVector& WorldPos, float TextSize,
		const UFont& Atlas, UCameraComponent* CameraComponent);

	void BuildTextMesh(const FString& Text, float TextSize, const UFont& Atlas);
private:

	FRenderer* Renderer = nullptr;

	ComPtr<ID3D11RasterizerState> RasterizerState;

	TUniquePtr<FShader> TextShader = nullptr;
	FPipelineState PipelineState;

	TUniquePtr<FVertexBuffer> VertexBuffer;
	TUniquePtr<FIndexBuffer> IndexBuffer;

	TArray<FTextVertex> Vertices;
	TArray<uint32> Indices;

	TUniquePtr<FConstantBuffer> MVP;
	TUniquePtr<FConstantBuffer> ScreenPx;
	uint32 MaxCharacters = 64; // 대략 UUID 문자열 길이(32~36자) 여유있게
	uint32 MaxVertices = 0;
	uint32 MaxIndices = 0;
};
#pragma once

#include "Render/Buffer.h"

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
		const FFontAtlas& Atlas, UCameraComponent* CameraComponent);

private:
	void BuildTextMesh(const FString& Text, float TextSize, const FFontAtlas& Atlas,
		TArray<FTextVertex>& OutVertices, TArray<uint32>& OutIndices);

	FRenderer* Renderer = nullptr;

	ComPtr<ID3D11RasterizerState> RasterizerState;

	TSharedPtr<FShader> TextShader = nullptr;

	TSharedPtr<FVertexBuffer> VertexBuffer;
	TSharedPtr<FIndexBuffer> IndexBuffer;

	TSharedPtr<FConstantBuffer> MVP;
	TSharedPtr<FConstantBuffer> ScreenPx;
	uint32 MaxCharacters = 64; // 대략 UUID 문자열 길이(32~36자) 여유있게
	uint32 MaxVertices = 0;
	uint32 MaxIndices = 0;
};
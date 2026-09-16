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
	void Init(FRenderer* InRenderer, uint32 InMaxCharacters = 256);

	// 컴포넌트 트랜스폼을 그대로 사용 (TextRenderComponent)
	void OnRender(const FString& Text, const FMatrix& WorldMatrix, float TextSize,
		const UFont& Atlas, UCameraComponent* CameraComponent);

	// 항상 카메라를 향하게 그린다 (에디터 UUID 라벨 등)
	void OnRenderBillboard(const FString& Text, const FVector& WorldPos, float TextSize,
		const UFont& Atlas, UCameraComponent* CameraComponent);

private:
	void BuildTextMesh(const FString& Text, float TextSize, const UFont& Atlas);

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
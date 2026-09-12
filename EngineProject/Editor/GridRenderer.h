#pragma once

#include <d3d11.h>
#include "Render/Renderer.h"

struct FGridData
{
	FMatrix invViewProj;
	FVector CameraPos;
	float padding;
};

class FGridRenderer
{
public:
	~FGridRenderer();

	bool Init(FRenderer* InRenderer);
	void OnRender(const FMatrix& ViewProj, const FVector& CameraPos);

	void SetGridSpace(float Value) { GridSpace = Value; }

private:
	FRenderer* Renderer;

	TSharedPtr<FShader> BatchGridShader;
	FVertex* VertexBufferBase;
	FVertex* VertexBufferPtr;
	TSharedPtr<FConstantBuffer> CB;
	uint32 VertexCount;

	uint32 MaxVertices = 0;
	TSharedPtr<FVertexBuffer> VertexBuffer;

	float GridSpace = 1.0f;

	//PS Grid용(주석만 풀면 됨)
	//TSharedPtr<FShader> GridShader;
	//FShader* AxisShader;

	//TSharedPtr<FVertexBuffer> VB;
	//TSharedPtr<FIndexBuffer> IB;
	//TSharedPtr<FConstantBuffer> CB;

	//Microsoft::WRL::ComPtr<ID3D11RasterizerState> RasterizerState;
};
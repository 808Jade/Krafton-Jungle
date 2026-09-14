#pragma once

#include <d3d11.h>
#include "Render/Renderer.h"

struct FGridData
{
	FMatrix invViewProj;
	FVector CameraPos;
	int32 CellSize;
	float SubCellSize;
	int32 GridPlaneType;
	float Padding[2];
};

struct BatchGridData
{
	FMatrix invViewProj;
	FVector CameraPos;
	float Padding;
};


struct FEditorSettings;

class FGridRenderer
{
public:
	~FGridRenderer();

	bool Init(FRenderer* InRenderer);

	//void OnRender(const FMatrix& ViewProj, const FVector& CameraPos, const FEditorSettings& InEditorSettings);
	void OnRender(const FMatrix& ViewProj, const FVector& CameraPos);

	void SetGridSpace(float Value) { GridSpace = Value; }

private:
	FRenderer* Renderer;

	// Batch Grid
	TSharedPtr<FShader> BatchGridShader;
	FVertex* BatchGridVertexBuffer;
	TSharedPtr<FConstantBuffer> CB;

	uint32 MaxVertices = 0;
	TSharedPtr<FVertexBuffer> VertexBuffer;

	float GridSpace = 1.0f;

	// PS Grid
	//TSharedPtr<FShader> GridShader;
	//FShader* AxisShader;

	//TSharedPtr<FVertexBuffer> VB;
	//TSharedPtr<FIndexBuffer> IB;
	//TSharedPtr<FConstantBuffer> CB;

	//Microsoft::WRL::ComPtr<ID3D11RasterizerState> RasterizerState;
};
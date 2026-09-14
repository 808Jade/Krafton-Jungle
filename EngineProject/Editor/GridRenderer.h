#pragma once

#include <d3d11.h>
#include "Render/Renderer.h"

struct FPSGridData
{
	FMatrix invViewProj;
	FVector CameraPos;
	int32 CellSize;
	float SubCellSize;
	int32 GridPlaneType;
	float Padding[2];
};

struct FBatchGridData
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

	void OnRenderPSGrid(const FMatrix& ViewProj, const FVector& CameraPos, const FEditorSettings& InEditorSettings);
	void OnRenderBatchGrid(const FMatrix& ViewProj, const FVector& CameraPos);

	void SetGridSpace(float Value) { BatchGridSpace = Value; }

private:
	FRenderer* Renderer;

	// Batch Grid
	TSharedPtr<FShader>			BatchGridShader;
	TSharedPtr<FVertexBuffer>	BatchGridVertexBuffer;
	TSharedPtr<FConstantBuffer>	BatchGridConstantBuffer;
	FVertex*					BatchGridVertices;
	uint32						MaxVertices = 0;
	float						BatchGridSpace = 1.0f;

	// PS Grid
	TSharedPtr<FShader>								PSGridShader;
	FShader*										AxisShader;
	TSharedPtr<FVertexBuffer>						PSGridVertexBuffer;
	TSharedPtr<FIndexBuffer>						PSGridIndexBuffer;
	TSharedPtr<FConstantBuffer>						PSGridConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState>	RasterizerState;
};
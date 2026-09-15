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
	void OnRender(const FMatrix& ViewProj, const FVector& CameraPos);

	void OnRenderPSGrid(const FMatrix& ViewProj, const FVector& CameraPos, const FEditorSettings& InEditorSettings);
	void OnRenderBatchGrid(const FMatrix& ViewProj, const FVector& CameraPos);

	void SetGridSpace(float Value) { BatchGridSpace = Value; }

private:
	FRenderer* Renderer;

	// Batch Grid
	TUniquePtr<FShader>			BatchGridShader;
	TUniquePtr<FVertexBuffer>	BatchGridVertexBuffer;
	TUniquePtr<FConstantBuffer>	BatchGridConstantBuffer;
	FVertex* BatchGridVertices;
	uint32						MaxVertices = 0;
	float						BatchGridSpace = 1.0f;

	// PS Grid
	TUniquePtr<FShader>								PSGridShader;
	TUniquePtr<FShader> AxisShader;
	TUniquePtr<FVertexBuffer>						PSGridVertexBuffer;
	TUniquePtr<FIndexBuffer>						PSGridIndexBuffer;
	TUniquePtr<FConstantBuffer>						PSGridConstantBuffer;
	ComPtr<ID3D11RasterizerState>	RasterizerState;
};
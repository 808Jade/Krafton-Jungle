#pragma once

#include <d3d11.h>
#include "Render/Renderer.h"

struct GridData
{
	FMatrix invViewProj;
	FVector CameraPos;
	int32 CellSize;
	float SubCellSize;
	int32 GridPlaneType;
	float Padding[2];
};
struct FEditorSettings;

class FGridRenderer
{
public:
	~FGridRenderer();

	bool Init(FRenderer* InRenderer);
	void OnRender(const FMatrix& ViewProj, const FVector& CameraPos, const FEditorSettings& InEditorSettings);
	
private:
	FRenderer* Renderer;

	TSharedPtr<FShader> GridShader;
	FShader* AxisShader;

	TSharedPtr<FVertexBuffer> VB;
	TSharedPtr<FIndexBuffer> IB;
	TSharedPtr<FConstantBuffer> CB;

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> RasterizerState;
};
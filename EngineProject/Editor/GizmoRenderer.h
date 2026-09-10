#pragma once

#include "Render/Renderer.h"
#include "Gizmo.h"

class UCameraComponent;

struct FGizmoData
{
	FMatrix World;
	FMatrix ViewProj;
	FVector4 Color;
};

struct FAxisData
{
	FRotator Rotator;
	FVector4 Color;
};

class FGizmoRenderer
{
public:
	FGizmoRenderer();
	~FGizmoRenderer() = default;

	bool Init(FRenderer* InRenderer);
	void OnRender(const FGizmo& Gizmo, const FMatrix& ViewProj);

private:
	void DrawMesh(FMesh* Mesh, const FGizmoData& Data);

	FRenderer* Renderer;

	FTransform Transform;
	
	TArray<FAxisData> AxisDataArray;

	TSharedPtr<FShader> Shader;
	TSharedPtr<FMesh> LocationMesh;
	TSharedPtr<FMesh> RotationMesh;
	TSharedPtr<FMesh> ScaleMesh;
	TSharedPtr<FMesh> SphereMesh;
	TSharedPtr<FConstantBuffer> CB;

	//EGizmoMode Mode = EGizmoMode::Scale;
};
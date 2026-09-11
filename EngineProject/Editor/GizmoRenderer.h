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
	void DrawMesh(UStaticMesh* Mesh, const FGizmoData& Data);

	FRenderer* Renderer;

	FTransform Transform;
	
	TArray<FAxisData> AxisDataArray;

	TSharedPtr<FShader> Shader;
	TSharedPtr<UStaticMesh> LocationMesh;
	TSharedPtr<UStaticMesh> RotationMesh;
	TSharedPtr<UStaticMesh> ScaleMesh;
	TSharedPtr<UStaticMesh> SphereMesh;
	TSharedPtr<FConstantBuffer> CB;

	//EGizmoMode Mode = EGizmoMode::Scale;
};
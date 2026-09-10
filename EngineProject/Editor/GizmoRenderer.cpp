#include "EnginePCH.h"
#include "GizmoRenderer.h"

#include "Render/GeometryGenerator.h"
#include "Camera/CameraComponent.h"

FGizmoRenderer::FGizmoRenderer()
{
}

bool FGizmoRenderer::Init(FRenderer* InRenderer)
{
	Renderer = InRenderer;
	FMeshData LocMeshData = FGeometryGenerator::CreateArrow(0.1f, 1.0f, 0.2f, 0.5f, 20, FVector4(1.0f, 0.0f, 0.0f, 1.0f));
	LocationMesh = MakeShared<FMesh>();
	LocationMesh->VertexBuffer = Renderer->CreateVertexBuffer(LocMeshData.Vertices.data(), sizeof(FVertex) * (UINT)LocMeshData.Vertices.size(), sizeof(FVertex));
	LocationMesh->IndexBuffer = Renderer->CreateIndexBuffer(LocMeshData.Indices.data(), LocMeshData.Indices.size());

	FMeshData RotMeshData = FGeometryGenerator::CreateRing(1.0f, 0.03f, 32, 16, FVector4(1.0f, 0.0f, 0.0f, 1.0f));
	RotationMesh = MakeShared<FMesh>();
	RotationMesh->VertexBuffer = Renderer->CreateVertexBuffer(RotMeshData.Vertices.data(), sizeof(FVertex) * (UINT)RotMeshData.Vertices.size(), sizeof(FVertex));
	RotationMesh->IndexBuffer = Renderer->CreateIndexBuffer(RotMeshData.Indices.data(), RotMeshData.Indices.size());

	FMeshData ScaleMeshData = FGeometryGenerator::CreateScaleBar(0.1f, 1.0f, 0.4f, 20, FVector4(1.0f, 0.0f, 0.0f, 1.0f));
	ScaleMesh = MakeShared<FMesh>();
	ScaleMesh->VertexBuffer = Renderer->CreateVertexBuffer(ScaleMeshData.Vertices.data(), sizeof(FVertex) * (UINT)ScaleMeshData.Vertices.size(), sizeof(FVertex));
	ScaleMesh->IndexBuffer = Renderer->CreateIndexBuffer(ScaleMeshData.Indices.data(), ScaleMeshData.Indices.size());

	FMeshData SphereData = FGeometryGenerator::CreateSphere(0.2f, 20, 10, FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	SphereMesh = MakeShared<FMesh>();
	SphereMesh->VertexBuffer = Renderer->CreateVertexBuffer(SphereData.Vertices.data(), sizeof(FVertex) * (UINT)SphereData.Vertices.size(), sizeof(FVertex));
	SphereMesh->IndexBuffer = Renderer->CreateIndexBuffer(SphereData.Indices.data(), SphereData.Indices.size());

	CB = Renderer->CreateConstantBuffer(sizeof(FGizmoData));

	AxisDataArray.push_back({ FRotator(90.0f, 0.0f, 0.0f) ,FVector4(1.0f, 0.0f, 0.0f, 1.0f) });
	AxisDataArray.push_back({ FRotator(0.0f, 0.0f, -90.0f) ,FVector4(0.0f, 1.0f, 0.0f, 1.0f) });
	AxisDataArray.push_back({ FRotator(0.0f, 0.0f, 0.0f) ,FVector4(0.0f, 0.4f, 1.0f, 1.0f) });

	AxisDataArray.push_back({ FRotator(0.0f, 0.0f, 0.0f) ,FVector4(0.0f, 0.0f, 1.0f, 1.0f) });
	AxisDataArray.push_back({ FRotator(0.0f, 0.0f, 0.0f) ,FVector4(0.0f, 0.0f, 1.0f, 1.0f) });
	AxisDataArray.push_back({ FRotator(0.0f, 0.0f, 0.0f) ,FVector4(0.0f, 0.0f, 1.0f, 1.0f) });
	AxisDataArray.push_back({ FRotator(0.0f, 0.0f, 0.0f) ,FVector4(1.0f, 1.0f, 1.0f, 1.0f) }); // ScreenAxis

	Shader = Renderer->CreateShader(L"Shader/GizmoShader.hlsl", FVertex::GetLayout());

	return false;
}

void FGizmoRenderer::OnRender(const FGizmo& Gizmo, const FMatrix& ViewProj)
{
	if (!Gizmo.GetTarget())
		return;

	FMesh* AxisMesh = nullptr;
	switch (Gizmo.GetMode())
	{
	case EGizmoMode::Location: AxisMesh = LocationMesh.get(); break;
	case EGizmoMode::Rotation: AxisMesh = RotationMesh.get(); break;
	case EGizmoMode::Scale:    AxisMesh = ScaleMesh.get();    break;
	default: return;
	}

	Renderer->BindShader(Shader.get());
	Renderer->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	const FMatrix ViewProjT = ViewProj.GetTransposed();
	// const FVector GizmoLocation = Gizmo.GetLocation();
	const int HoveredAxis = Gizmo.GetHoveredAxis();

	//const FVector GizmoLocation = (Gizmo.GetTarget()->GetTransform()->Location - CameraComponent->GetTransform()->Location).Normalize() * 10.0f + CameraComponent->GetTransform()->Location;
	const FVector GizmoLocation = Gizmo.GetRenderLocation();
	// 축 3개
	Renderer->BindMesh(AxisMesh);
	Transform.Location = GizmoLocation;

	for (int i = 0; i < 3; ++i)
	{
		FMatrix World;

		if (Gizmo.GetMode() == EGizmoMode::Scale || Gizmo.GetSpace() == EGizmoSpace::Local)
		{
			FMatrix AxisRot = AxisDataArray[i].Rotator.Quaternion().ToFMatrix();
			FMatrix ObjRot = Gizmo.GetRotation().Quaternion().ToFMatrix();
			FMatrix Trans = FMatrix::MakeTranslation(GizmoLocation);
			World = AxisRot * ObjRot * Trans;
		}
		else
		{
			Transform.Location = GizmoLocation;
			Transform.Rotation = AxisDataArray[i].Rotator;
			World = Transform.GetWorldMatrix();
		}

		FGizmoData Data{};
		Data.World = World.GetTransposed();
		Data.ViewProj = ViewProjT;
		Data.Color = (i == HoveredAxis)
			? FVector4(1.0f, 1.0f, 0.0f, 1.0f)     // hover 시 노랑
			: AxisDataArray[i].Color;

		DrawMesh(AxisMesh, Data);
	}

	if (Gizmo.GetMode() != EGizmoMode::Rotation)
	{
		// 중앙 구
		Transform.Rotation = FRotator(0.0f, 0.0f, 0.0f);

		FGizmoData SphereData{};
		SphereData.World = Transform.GetWorldMatrix().GetTransposed();
		SphereData.ViewProj = ViewProjT;
		SphereData.Color = (6 == HoveredAxis)
			? FVector4(1.0f, 1.0f, 0.0f, 1.0f)     // hover 시 노랑
			: AxisDataArray[6].Color;

		DrawMesh(SphereMesh.get(), SphereData);
	}
	else
	{
		FVector CamPos = Gizmo.GetCameraLocation();
		FVector Forward = (GizmoLocation - CamPos).Normalize();
		FVector Right = FVector(0, 0, 1).Cross(Forward).Normalize();
		FVector Up = Forward.Cross(Right);

		FMatrix Rot = Identity;
		Rot.M[0][0] = Right.X;   Rot.M[0][1] = Right.Y;   Rot.M[0][2] = Right.Z;
		Rot.M[1][0] = Up.X;      Rot.M[1][1] = Up.Y;      Rot.M[1][2] = Up.Z;
		Rot.M[2][0] = Forward.X; Rot.M[2][1] = Forward.Y; Rot.M[2][2] = Forward.Z;

		FMatrix Scale = Identity;
		Scale.M[0][0] = Scale.M[1][1] = Scale.M[2][2] = 1.3f;

		FMatrix Trans = FMatrix::MakeTranslation(GizmoLocation);

		FGizmoData Data{};
		Data.World = (Scale * Rot * Trans).GetTransposed();
		Data.ViewProj = ViewProjT;
		Data.Color = (6 == HoveredAxis)
			? FVector4(1.0f, 1.0f, 0.0f, 1.0f)     // hover 시 노랑
			: AxisDataArray[6].Color;


		DrawMesh(RotationMesh.get(), Data);
	}
}

void FGizmoRenderer::DrawMesh(FMesh* Mesh, const FGizmoData& Data)
{
	Renderer->BindMesh(Mesh);
	Renderer->UpdateConstantBufferData(CB.get(), &Data, sizeof(FGizmoData));
	Renderer->BindConstantBuffer(0, CB.get(), EShaderBindFlagBits::Vertex);
	Renderer->DrawIndexed(Mesh->IndexBuffer->GetIndexCount());
}

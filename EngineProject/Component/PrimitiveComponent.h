#pragma once

#include "SceneComponent.h"
#include "../Render/Shader.h"
#include "../Render/Mesh.h"
#include "Render/RenderPacket.h"
#include "Render/GeometryGenerator.h"

enum class EPrimitiveType
{
	Sphere,
	Cube,
	Cone,
	Plane
};

class UPrimitiveComponent :public USceneComponent
{
	DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

	REFLECT_START(ClassName)
	REFLECT_END()
public:
	virtual void BeginPlay() override;

	void SetMeshData(const FMeshData& data) { MeshData = data; }
	void SetMeshShader(FMesh* InMesh, FShader* InShader) { Mesh = InMesh; Shader = InShader; }
	void SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue);

	FMeshData GetMeshData() const { return MeshData; }
	void SetMesh(FMesh* InMesh);
	FMesh* GetMesh() const { return Mesh; };
	FShader* GetShader() const { return Shader; };

	void SetType(EPrimitiveType Ptype) { Type = Ptype; }
	EPrimitiveType GetType() const { return Type; }
protected:
	EPrimitiveType Type = EPrimitiveType::Cube;
	FShader* Shader = nullptr;
	FMesh* Mesh = nullptr;
	FMeshData MeshData;
};
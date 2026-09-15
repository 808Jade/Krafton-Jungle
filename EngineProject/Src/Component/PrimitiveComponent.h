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
	Plane,

	// Todo: subuv
	Particle
};

class UPrimitiveComponent :public USceneComponent
{
	DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

	REFLECT_START(ClassName)
	REFLECT_END()
public:
	virtual void BeginPlay() override;

	void ApplyPrimitiveType(EPrimitiveType Type);

	void SetMeshData(const FStaticMeshData& data) { MeshData = data; }
	//void SetMeshShader(UStaticMesh* InMesh, FShader* InShader) { Mesh = InMesh; Shader = InShader; }
	
	// Todo: subuv
	//void SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue);
	virtual void SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue);

	FStaticMeshData GetMeshData() const { return MeshData; }
	void SetMesh(UStaticMesh* InMesh);
	UStaticMesh* GetMesh() const { return Mesh; };
	FShader* GetShader() const { return Shader.get(); };

	EPrimitiveType GetType() const { return Type; }
	void SetType(EPrimitiveType Ptype) { Type = Ptype; }

protected:
	EPrimitiveType Type = EPrimitiveType::Cube;
	TUniquePtr<FShader> Shader = nullptr;
	UStaticMesh* Mesh = nullptr;
	FStaticMeshData MeshData;
};
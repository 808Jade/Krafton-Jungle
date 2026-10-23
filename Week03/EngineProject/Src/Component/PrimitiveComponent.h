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
	Particle,
	ParticleQuad
};

class UPrimitiveComponent :public USceneComponent
{
	DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

	REFLECT_START(ClassName)
	PROPERTY(Material)
	REFLECT_END()
public:
	virtual void BeginPlay() override;

	void ApplyPrimitiveType(EPrimitiveType Type);

	// Todo: subuv
	//void SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue);
	virtual void SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue);

	inline void SetMeshData(const FStaticMeshData& data) { MeshData = data; }
	inline FStaticMeshData GetMeshData() const { return MeshData; }

	inline void SetMesh(UStaticMesh* InMesh) { Mesh = InMesh; }
	inline UStaticMesh* GetMesh() const { return Mesh; };

	inline void SetMaterial(UMaterial* InMaterial) { Material = InMaterial; }
	inline UMaterial* GetMaterial() const { return Material; };
	//
	// FShader* GetShader() const { return Shader.get(); };

	EPrimitiveType GetType() const { return Type; }
	void SetType(EPrimitiveType Ptype) { Type = Ptype; }

protected:
	EPrimitiveType Type = EPrimitiveType::Cube;
	UStaticMesh* Mesh = nullptr;
	UMaterial* Material = nullptr;
	FStaticMeshData MeshData;
};
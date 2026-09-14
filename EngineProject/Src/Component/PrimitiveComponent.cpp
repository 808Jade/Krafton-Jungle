#include "EnginePCH.h"
#include "PrimitiveComponent.h"
#include "../Render/Renderer.h"
#include "Asset/AssetManager.h"

namespace
{
	FString PrimitiveTypeToString(EPrimitiveType Type)
	{
		switch (Type)
		{
		case EPrimitiveType::Sphere:
			return "Sphere";
			break;
		case EPrimitiveType::Cube:
			return "Cube";
			break;
		case EPrimitiveType::Cone:
			return "Cone";
			break;
		case EPrimitiveType::Plane:
			return "Plane";
			break;
		default:
			return "";
			break;
		}
	}
}

void UPrimitiveComponent::BeginPlay()
{
	Super::BeginPlay();
	Shader = UAssetManager::Get().GetShader("Shader/DefaultShader.hlsl");
}

void UPrimitiveComponent::ApplyPrimitiveType(EPrimitiveType Type)
{
	SetType(Type);
	SetMesh(UAssetManager::Get().GetMesh(PrimitiveTypeToString(Type)));
	SetMeshData(*FGeometryGenerator::GetMeshData(PrimitiveTypeToString(Type)));
}

void UPrimitiveComponent::SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue)
{
	if (Mesh && Shader)
	{
		FRenderPacket rp;
		rp.mesh = Mesh;
		rp.shader = Shader;
		rp.model = GetWorldMatrix();

		// Todo: subuv
		rp.bSubUV = false;

		RenderQueue.Enqueue(rp);
	}
}

void UPrimitiveComponent::SetMesh(UStaticMesh* InMesh)
{
	Mesh = InMesh;
}

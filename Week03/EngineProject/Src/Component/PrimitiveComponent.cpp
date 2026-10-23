#include "EnginePCH.h"
#include "PrimitiveComponent.h"
#include "../Render/Renderer.h"
#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"

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
	SetMaterial(UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial"));
	GetOwner()->GetWorld()->AddPrimitive(this);
}

void UPrimitiveComponent::ApplyPrimitiveType(EPrimitiveType Type)
{
	SetType(Type);
	SetMesh(UAssetManager::GetAssetByPath<UStaticMesh>(PrimitiveTypeToString(Type)));
	SetMeshData(*FGeometryGenerator::GetMeshData(PrimitiveTypeToString(Type)));
}

void UPrimitiveComponent::SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue)
{
	if (Mesh && Material)
	{
		FRenderPacket rp;
		rp.mesh = Mesh;
		rp.material = Material;
		rp.model = GetWorldMatrix();

		// Todo: subuv
		//rp.bSubUV = false;

		RenderQueue.Enqueue(rp);
	}
}




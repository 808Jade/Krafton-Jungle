#include "EnginePCH.h"
#include "StaticMeshActor.h"

#include "Component/PrimitiveComponent.h"
#include "ObjectSystem/ObjectFactory.h"
#include "Asset/AssetManager.h"

namespace
{
	FString PrimitiveTypeToString(EPrimitiveType Type)
	{
		switch (Type)
		{
		case EPrimitiveType::Cube:
			return "Cube";

		case EPrimitiveType::Sphere:
			return "Sphere";

		case EPrimitiveType::Plane:
			return "Plane";

		case EPrimitiveType::Cone:
			return "Cone";

		default:
			return "";
		}
	}
}

AStaticMeshActor::AStaticMeshActor()
{
	PrimitiveComponent = CreateDefaultSubobject<UPrimitiveComponent>();
	SetRootComponent(PrimitiveComponent);
}

void AStaticMeshActor::SetPrimitiveType(EPrimitiveType Type)
{
	UPrimitiveComponent* Component = GetPrimitiveComponent();

	Component->SetType(Type);
	Component->SetMesh(UAssetManager::GetAssetByPath<UStaticMesh>(PrimitiveTypeToString(Type)));
	FStaticMeshData* MeshData = FGeometryGenerator::GetMeshData(PrimitiveTypeToString(Type));

	if (MeshData)
	{
		Component->SetMeshData(*MeshData);
	}
}

void AStaticMeshActor::BeginPlay()
{
	Super::BeginPlay();
	//GetWorld()->AddPrimitive(PrimitiveComponent);
}

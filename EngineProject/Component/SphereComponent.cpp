#include "EnginePCH.h"
#include "SphereComponent.h"
#include "Engine/ResourceManager.h"

void USphereComponent::BeginPlay()
{
	Super::BeginPlay();
	SetMesh(FResourceManager::GetInstance().GetMesh("Sphere"));
	SetMeshData(FGeometryGenerator::GetMeshData("Sphere"));
}

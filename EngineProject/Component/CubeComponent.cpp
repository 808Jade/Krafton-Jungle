#include "EnginePCH.h"
#include "CubeComponent.h"
#include "../Engine/ResourceManager.h"

void UCubeComponent::BeginPlay()
{
	Super::BeginPlay();
	SetMesh(FResourceManager::GetInstance().GetMesh("Cube"));
	SetMeshData(FGeometryGenerator::GetMeshData("Cube"));
}

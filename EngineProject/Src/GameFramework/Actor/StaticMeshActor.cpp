#include "EnginePCH.h"
#include "StaticMeshActor.h"

#include "Component/PrimitiveComponent.h"
#include "ObjectSystem/ObjectFactory.h"

AStaticMeshActor::AStaticMeshActor()
{
	PrimitiveComponent = CreateDefaultSubobject<UPrimitiveComponent>();

	SetRootComponent(PrimitiveComponent);
}

void AStaticMeshActor::BeginPlay()
{
	Super::BeginPlay();
	GetWorld()->AddPrimitive(PrimitiveComponent);
}

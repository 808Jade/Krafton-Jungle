#include "EnginePCH.h"
#include "Actor.h"

#include "../ObjectSystem/ObjectFactory.h"
#include "../Engine/World.h"

#include "../Component/CubeComponent.h"
#include "../Component/SphereComponent.h"
#include "../Engine/ResourceManager.h"

AActor::AActor()
{
	
}

AActor::~AActor()
{
	//for (UActorComponent* Component : Components)
	//	delete Component;
	//Components.clear();
	//RootComponent = nullptr;   // Root 는 Components에 이미 들어있으므로 delete 하지 말 것
}

void AActor::BeginPlay()
{
	if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(RootComponent))
	{
		World->AddPrimitive(Cast<UPrimitiveComponent>(RootComponent));
	}

	for (UActorComponent* Component : Components)
	{
		Component->BeginPlay();
	}

}

void AActor::Tick(float DeltaTime)
{
	for (UActorComponent* Component : Components)
	{
		Component->TickComponent(DeltaTime);
	}
}

void AActor::AddPrimitiveComponent(EPrimitiveType Type, FTransform Transform)
{
	switch (Type)
	{
	case EPrimitiveType::Sphere:
		RootComponent = FObjectFactory::ConstructObject<USphereComponent>();
		Cast<UPrimitiveComponent>(RootComponent)->SetType(Type);
		break;
	case EPrimitiveType::Cube:
		RootComponent = FObjectFactory::ConstructObject<UCubeComponent>();
		Cast<UPrimitiveComponent>(RootComponent)->SetType(Type);
		break;
	case EPrimitiveType::Cone:
		RootComponent = FObjectFactory::ConstructObject<UPrimitiveComponent>();
		Cast<UPrimitiveComponent>(RootComponent)->SetMesh(FResourceManager::GetInstance().GetMesh("Cone"));
		Cast<UPrimitiveComponent>(RootComponent)->SetMeshData(FGeometryGenerator::GetMeshData("Cone"));
		Cast<UPrimitiveComponent>(RootComponent)->SetType(Type);
		break;
	case EPrimitiveType::Plane:
		RootComponent = FObjectFactory::ConstructObject<UPrimitiveComponent>();
		Cast<UPrimitiveComponent>(RootComponent)->SetMesh(FResourceManager::GetInstance().GetMesh("Plane"));
		Cast<UPrimitiveComponent>(RootComponent)->SetMeshData(FGeometryGenerator::GetMeshData("Plane"));
		Cast<UPrimitiveComponent>(RootComponent)->SetType(Type);
		break;
	default:
		break;
	break;
	}
	RootComponent->SetTransform(Transform);
	Components.push_back(RootComponent);
}

void AActor::SetRootComponent(USceneComponent* SceneComponent)
{
	RootComponent = SceneComponent;
}

USceneComponent* AActor::GetRootComponent()
{
	return RootComponent;
}
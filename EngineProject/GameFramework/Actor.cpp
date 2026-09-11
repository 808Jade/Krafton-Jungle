#include "EnginePCH.h"
#include "Actor.h"

#include "ObjectSystem/ObjectFactory.h"
#include "Engine/World.h"

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



void AActor::SetRootComponent(USceneComponent* SceneComponent)
{
	RootComponent = SceneComponent;
}

USceneComponent* AActor::GetRootComponent()
{
	return RootComponent;
}
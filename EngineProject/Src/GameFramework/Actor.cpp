#include "EnginePCH.h"
#include "Actor.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "ObjectSystem/ObjectFactory.h"

AActor::AActor()
{
}

AActor::~AActor()
{
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
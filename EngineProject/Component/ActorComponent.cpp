#include "EnginePCH.h"

#include "GameFramework/Actor.h"

void UActorComponent::SetOwner(AActor* InOwner)
{
	Owner = InOwner;
}

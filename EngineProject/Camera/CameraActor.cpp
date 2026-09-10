#include "EnginePCH.h"

#include "CameraActor.h"
#include "../Component/ActorComponent.h"
#include "CameraComponent.h"
#include "ObjectSystem/ObjectFactory.h"

ACameraActor::ACameraActor()
{
	UCameraComponent* CameraComponent = FObjectFactory::ConstructObject<UCameraComponent>();
	SetRootComponent(Cast<USceneComponent>(CameraComponent));
	Components.push_back(Cast<UActorComponent>(CameraComponent));
}

ACameraActor::~ACameraActor()
{
	// delete CameraComponent;

	//for (auto& i : SceneComponents)
	//{
	//	delete i;
	//}
}

void ACameraActor::BeginPlay()
{
	Super::BeginPlay();
}

void ACameraActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	/*CameraComponent->TickComponent(DeltaTime);

	for (auto& i : SceneComponents)
	{
		i->TickComponent(DeltaTime);
	}*/
}

UCameraComponent* ACameraActor::GetCameraComponent()
{
	for (UActorComponent* Component : Components)
	{
		if (Cast<UCameraComponent>(Component))
		{
			return Cast<UCameraComponent>(Component);
		}
	}

	return nullptr;
}
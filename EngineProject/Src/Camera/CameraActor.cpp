#include "EnginePCH.h"

#include "CameraActor.h"
#include "../Component/ActorComponent.h"
#include "CameraComponent.h"
#include "ObjectSystem/ObjectFactory.h"

ACameraActor::ACameraActor()
{
	CameraComponent = CreateDefaultSubobject<UCameraComponent>();
	SetRootComponent(CameraComponent);
}
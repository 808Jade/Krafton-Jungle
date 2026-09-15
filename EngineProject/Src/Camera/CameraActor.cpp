#include "EnginePCH.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

#include "../Component/ActorComponent.h"
#include "../ObjectSystem/ObjectFactory.h"

ACameraActor::ACameraActor()
{
	CameraComponent = CreateDefaultSubobject<UCameraComponent>();
	SetRootComponent(CameraComponent);
}
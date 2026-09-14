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

ACameraActor::~ACameraActor()
{
}

void ACameraActor::BeginPlay()
{
	Super::BeginPlay();
}

void ACameraActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

UCameraComponent* ACameraActor::GetCameraComponent()
{
	return CameraComponent;
}
#pragma once

#include "../GameFramework/Actor.h"
#include "../Engine/World.h"
#include "../Engine/Level.h"

class UCameraComponent;

class ACameraActor : public AActor
{
	DECLARE_CLASS(ACameraActor, AActor)

public:
	ACameraActor();

	UCameraComponent* GetCameraComponent() const { return CameraComponent; };

private:
	UCameraComponent* CameraComponent;
};

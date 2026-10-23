#pragma once

#include "../GameFramework/Actor.h"
#include "ObjectSystem/Class.h"

#include "CameraComponent.h"

//class UCameraComponent;

class ACameraActor : public AActor
{
	DECLARE_CLASS(ACameraActor, AActor)

public:
	ACameraActor();

	UCameraComponent* GetCameraComponent() const { return CameraComponent; };

private:
	UCameraComponent* CameraComponent;
};

#pragma once

#include "../GameFramework/Actor.h"

class UCameraComponent;
	
class ACameraActor : public AActor
{
	DECLARE_CLASS(ACameraActor, AActor)
public:
	ACameraActor();
	virtual ~ACameraActor() override;
protected:
	TArray<USceneComponent*> SceneComponents;

public:
	//virtual USceneComponent* GetDefaultAttachComponent(); // override ? <- Need to override Actors one in UE
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	UCameraComponent* GetCameraComponent(); // return CameraComponent subobject
};

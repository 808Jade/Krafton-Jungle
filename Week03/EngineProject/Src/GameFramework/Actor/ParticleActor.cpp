#include "EnginePCH.h"
#include "ParticleActor.h"

// Todo: subuv
AParticleActor::AParticleActor()
{
	RootComponent = CreateDefaultSubobject<UParticleSubUVComponent>();
}

UParticleSubUVComponent* AParticleActor::GetParticleComponent() const
{
	return static_cast<UParticleSubUVComponent*>(RootComponent);
}



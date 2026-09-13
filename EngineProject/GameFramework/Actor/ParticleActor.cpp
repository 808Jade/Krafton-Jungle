#include "EnginePCH.h"
#include "ParticleActor.h"

// Todo: subuv
AParticleActor::AParticleActor()
{
	ParticleComponent = CreateDefaultSubobject<UParticleSubUVComponent>();
	SetRootComponent(ParticleComponent);
}

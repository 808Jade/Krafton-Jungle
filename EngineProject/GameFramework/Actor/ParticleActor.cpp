#include "EnginePCH.h"
#include "ParticleActor.h"

// Todo: subuv
AParticleActor::AParticleActor()
{
	//ParticleComponent = CreateDefaultSubobject<UParticleSubUVComponent>();
	//SetRootComponent(ParticleComponent);

	RootComponent = CreateDefaultSubobject<UParticleSubUVComponent>();
	//SetRootComponent(ParticleComponent);
}

UParticleSubUVComponent* AParticleActor::GetParticleComponent() const
{
	return static_cast<UParticleSubUVComponent*>(RootComponent);
}

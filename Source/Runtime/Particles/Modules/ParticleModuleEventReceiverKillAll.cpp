#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleEventReceiverKillAll.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Components/ParticleSystemComponent.h"

void UParticleModuleEventReceiverKillAll::Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime)
{
	UParticleSystemComponent* Component = Owner.Component;
	if (!Component || Owner.ActiveParticles <= 0)
	{
		return;
	}

	for (const FParticleCollisionEvent& Event : Component->GetReceivedCollisionEvents())
	{
		if (ParticleEventNameMatches(EventName, Event.EventName))
		{
			Owner.KillParticlesForced();
			return;
		}
	}
}

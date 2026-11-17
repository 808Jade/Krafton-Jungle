#include "EnginePCH.h"
#include "Particles/ParticleLODLevel.h"

#include "Particles/ParticleModule.h"
#include "Particles/Modules/ParticleModuleRequired.h"
#include "Particles/Modules/ParticleModuleSpawn.h"
#include "Particles/Modules/ParticleModuleLifetime.h"
#include "UObject/Casts.h"

int32 UParticleLODLevel::CalculateMaxActiveParticleCount() const
{
	float MaxLifetime = 0.0f;
	bool bHasLifetimeModule = false;
	for (UParticleModule* Module : Modules)
	{
		if (!Module || !Module->bEnabled)
		{
			continue;
		}
		if (UParticleModuleLifetime* LifetimeModule = Cast<UParticleModuleLifetime>(Module))
		{
			MaxLifetime += LifetimeModule->GetMaxLifetime();
			bHasLifetimeModule = true;
		}
	}
	if (!bHasLifetimeModule)
	{
		MaxLifetime = 1.0f;
	}

	float Estimate = 0.0f;
	if (SpawnModule && SpawnModule->bEnabled)
	{
		Estimate = std::max(SpawnModule->SpawnRate, 0.0f) * std::max(MaxLifetime, 0.0f);
		for (const FParticleBurst& Burst : SpawnModule->Bursts)
		{
			Estimate += static_cast<float>(std::max(Burst.Count, 0));
		}
	}
	int32 Slots = static_cast<int32>(std::ceil(Estimate)) + 1;

	if (RequiredModule && RequiredModule->MaxActiveParticles > 0)
	{
		Slots = std::min(Slots, RequiredModule->MaxActiveParticles);
	}
	return Slots;
}

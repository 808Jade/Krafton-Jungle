#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleSubUVBase.h"
#include "Particles/Modules/ParticleModuleRequired.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleLODLevel.h"

int32 UParticleModuleSubUVBase::GetTotalFrames(const FParticleEmitterInstance& Owner)
{
	const UParticleModuleRequired* Required = Owner.CurrentLODLevel ? Owner.CurrentLODLevel->RequiredModule : nullptr;
	if (!Required)
	{
		return 1;
	}
	return std::max(Required->SubImagesHorizontal, 1) * std::max(Required->SubImagesVertical, 1);
}

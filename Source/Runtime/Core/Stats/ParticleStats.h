#pragma once

#include "LightweightStats.h"

namespace ParticleStats
{
	DECLARE_CYCLE_STAT("Particle Simulation CPU", STAT_SimulationCPU);
	DECLARE_CYCLE_STAT("Particle Render Data CPU", STAT_RenderDataCPU);
	DECLARE_CYCLE_STAT("Particle Sort CPU", STAT_SortCPU);
	DECLARE_CYCLE_STAT("Particle Render Submit CPU", STAT_RenderSubmitCPU);
	DECLARE_CYCLE_STAT("Particle Upload CPU", STAT_UploadCPU);

	// Frame counts; view work accumulates across views.
	DECLARE_COUNTER_STAT("Particle Systems", STAT_Systems);
	DECLARE_COUNTER_STAT("Particle Emitters", STAT_Emitters);
	DECLARE_COUNTER_STAT("Particle Active Particles", STAT_ActiveParticles);
	DECLARE_COUNTER_STAT("Particle Capacity", STAT_Capacity);
	DECLARE_COUNTER_STAT("Particle Visible Systems", STAT_VisibleSystems);
	DECLARE_COUNTER_STAT("Particle Submitted Emitters", STAT_SubmittedEmitters);
	DECLARE_COUNTER_STAT("Particle Spawned", STAT_Spawned);
	DECLARE_COUNTER_STAT("Particle Killed", STAT_Killed);
	DECLARE_COUNTER_STAT("Particle Spawn Dropped", STAT_SpawnDropped);
	DECLARE_COUNTER_STAT("Particle Draw Calls", STAT_DrawCalls);
	DECLARE_COUNTER_STAT("Particle Sorted Particles", STAT_SortedParticles);
	DECLARE_COUNTER_STAT("Particle Upload Bytes", STAT_UploadBytes);

	// Dedicated allocations, excluding shared assets.
	DECLARE_MEMORY_STAT("Particle CPU Memory", STAT_CPUMemory);
	DECLARE_MEMORY_STAT("Particle GPU Buffer Memory", STAT_GPUBufferMemory);

	inline void BeginFrame()
	{
		// Record zero samples when no particle work runs this frame.
		FStatRegistry::AddCycles(STAT_SimulationCPU, 0);
		FStatRegistry::AddCycles(STAT_RenderDataCPU, 0);
		FStatRegistry::AddCycles(STAT_SortCPU, 0);
		FStatRegistry::AddCycles(STAT_RenderSubmitCPU, 0);
		FStatRegistry::AddCycles(STAT_UploadCPU, 0);
		FStatRegistry::SetMemory(STAT_CPUMemory, 0);
	}
}

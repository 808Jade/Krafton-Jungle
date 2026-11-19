#include "EnginePCH.h"
#include "Render/RenderPacket.h"
#include "Core/Stats/ParticleStats.h"
#include "ParticleSystemSceneProxy.h"

#include "Components/ParticleSystemComponent.h"

FParticleSystemSceneProxy::FParticleSystemSceneProxy(
	UParticleSystemComponent* Component)
    : FPrimitiveSceneProxy(Component)
{
}

void FParticleSystemSceneProxy::UpdateDynamicData()
{
	SCOPE_CYCLE_COUNTER(ParticleStats::STAT_RenderDataCPU);
	MarkShadowDirty();
	const auto* Component =
		static_cast<const UParticleSystemComponent*>(GetComponent());

	DynamicEmitterData.SetNum(Component->EmitterInstances.Num());

	for (int32 Index = 0; Index < Component->EmitterInstances.Num(); ++Index)
	{
		TUniquePtr<FDynamicEmitterDataBase>& Data = DynamicEmitterData[Index];

		Data.reset(Component->EmitterInstances[Index]->GetDynamicData());
		if (!Data)
			continue;

		Data->EmitterIndex = Index;
		Data->PrepareRenderData(*this);
	}
}

bool FParticleSystemSceneProxy::GatherDynamicRenderPackets(
    FRenderQueue& RenderQueue,
    FRenderQueue*,
    const FRenderViewContext& View,
    FBillboardMatrixBuilder,
    const void*,
    TArray<int32>&,
    TArray<float>&) const
{
	FStatRegistry::AddCounter(ParticleStats::STAT_VisibleSystems);
	for (const TUniquePtr<FDynamicEmitterDataBase>& Data : DynamicEmitterData)
	{
		if (Data)
		{
			const uint32 Before = RenderQueue.Num();
			Data->GatherRenderPackets(*this, RenderQueue, View);
			if (RenderQueue.Num() > Before)
				FStatRegistry::AddCounter(ParticleStats::STAT_SubmittedEmitters);
		}
	}
	return true;
}

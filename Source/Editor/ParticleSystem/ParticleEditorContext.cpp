#include "EnginePCH.h"
#include "Editor/ParticleSystem/ParticleEditorContext.h"

#include "GameFramework/Actor/ParticleSystemActor.h"
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "UObject/UObjectIterator.h"

#include <algorithm>

FParticleEditorContext::~FParticleEditorContext()
{
	Release();
}

bool FParticleEditorContext::Init()
{
	PreviewScene = MakeUnique<FPreviewScene>("ParticlePreviewWorld");
	if (!PreviewScene->GetWorld())
		return false;

	PreviewActor = PreviewScene->SpawnActor<AParticleSystemActor>("PreviewParticleSystem");
	return PreviewActor != nullptr;
}

void FParticleEditorContext::Release()
{
	PreviewActor = nullptr;
	SelectedEmitterIndex = INDEX_NONE;
	SelectedModule = nullptr;
	if (PreviewScene)
		PreviewScene->Release();
	PreviewScene.reset();
}

void FParticleEditorContext::Tick(float DeltaTime)
{
	if (PreviewScene)
		PreviewScene->Tick(bSimulationPaused ? 0.0f : DeltaTime);
}

void FParticleEditorContext::RestartSimulation()
{
	if (!PreviewScene)
		return;

	// 삭제는 프리뷰 월드의 다음 Tick에서 처리된다. 새 액터는 빈 이미터로 시작한다.
	if (PreviewActor)
		PreviewScene->DestroyActor(PreviewActor);
	PreviewActor = PreviewScene->SpawnActor<AParticleSystemActor>();
}

int32 FParticleEditorContext::RestartLevelInstances()
{
	UParticleSystem* Template = GetTemplate();
	if (!Template)
		return 0;

	// 월드에 속한 컴포넌트만 대상으로 한다. 액터 CDO의 하위 객체처럼 월드 밖에 있는 객체는 시뮬레이션하지 않는다.
	int32 RestartedCount = 0;
	for (TObjectIterator<UParticleSystemComponent> It; It; ++It)
	{
		UParticleSystemComponent* Component = *It;
		if (!Component || Component->Template != Template)
			continue;
		const AActor* Owner = Component->GetOwner();
		if (!Owner || !Owner->GetWorld())
			continue;

		// bReset이면 InitParticles로 이미터 인스턴스를 새로 만든다. 모듈 구성이 바뀌었어도 반영된다.
		Component->ActivateSystem(true);
		++RestartedCount;
	}
	return RestartedCount;
}

int32 FParticleEditorContext::GetLODCount() const
{
	const UParticleSystem* Template = GetTemplate();
	int32 Count = 1;
	if (Template)
	{
		for (const UParticleEmitter* Emitter : Template->Emitters)
		{
			if (Emitter)
				Count = std::max(Count, Emitter->LODLevels.Num());
		}
	}
	return Count;
}

void FParticleEditorContext::SetCurrentLOD(const int32 NewLODIndex)
{
	CurrentLODIndex = std::clamp(NewLODIndex, 0, GetLODCount() - 1);
}

UParticleSystemComponent* FParticleEditorContext::GetPreviewComponent() const
{
	return PreviewActor ? PreviewActor->ParticleSystemComponent : nullptr;
}

UParticleSystem* FParticleEditorContext::GetTemplate() const
{
	const UParticleSystemComponent* Component = GetPreviewComponent();
	return Component ? Component->Template : nullptr;
}

FParticleEmitterInstance* FParticleEditorContext::GetSelectedEmitterInstance() const
{
	const UParticleSystemComponent* Component = GetPreviewComponent();
	if (!Component || SelectedEmitterIndex < 0 || SelectedEmitterIndex >= Component->EmitterInstances.Num())
		return nullptr;
	return Component->EmitterInstances[SelectedEmitterIndex];
}

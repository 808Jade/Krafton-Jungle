#include "EnginePCH.h"
#include "Editor/ParticleSystem/ParticleEditorContext.h"

#include "GameFramework/Actor/ParticleSystemActor.h"
#include "Components/ParticleSystemComponent.h"

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

void FParticleEditorContext::SetFixedBoundsFromCurrent()
{
	UParticleSystemComponent* Component = GetPreviewComponent();
	if (!Component)
		return;

	// 고정 바운드가 켜져 있으면 CalcBounds가 고정 값을 돌려주므로 잠시 꺼서 실제 파티클 영역을 얻는다.
	const bool bWasFixed = Component->bUseFixedRelativeBoundingBox;
	Component->bUseFixedRelativeBoundingBox = false;
	const FBox Current = Component->CalcBounds();
	Component->bUseFixedRelativeBoundingBox = bWasFixed;

	// 파티클이 없으면 원점 한 점이 나온다. 크기 0인 바운드로 덮어쓰지 않는다.
	const FVector Size = Current.Max - Current.Min;
	if (Size.X <= 0.0f && Size.Y <= 0.0f && Size.Z <= 0.0f)
		return;

	// 고정 바운드는 컴포넌트 기준 상대 좌표다. 프리뷰 액터는 원점·단위 변환에 있으므로 월드 바운드를 그대로 쓴다.
	Component->FixedRelativeBoundingBox = Current;
	Component->bUseFixedRelativeBoundingBox = true;
}

void FParticleEditorContext::UseDynamicBounds()
{
	if (UParticleSystemComponent* Component = GetPreviewComponent())
		Component->bUseFixedRelativeBoundingBox = false;
}

UParticleSystemComponent* FParticleEditorContext::GetPreviewComponent() const
{
	return PreviewActor ? PreviewActor->ParticleSystemComponent : nullptr;
}

FParticleEmitterInstance* FParticleEditorContext::GetSelectedEmitterInstance() const
{
	const UParticleSystemComponent* Component = GetPreviewComponent();
	if (!Component || SelectedEmitterIndex < 0 || SelectedEmitterIndex >= Component->EmitterInstances.Num())
		return nullptr;
	return Component->EmitterInstances[SelectedEmitterIndex];
}

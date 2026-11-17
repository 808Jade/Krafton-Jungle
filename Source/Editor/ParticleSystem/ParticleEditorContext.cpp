#include "EnginePCH.h"
#include "Editor/ParticleSystem/ParticleEditorContext.h"

#include "GameFramework/Actor/ParticleSystemActor.h"
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/Modules/ParticleModuleRequired.h"
#include "Particles/Modules/ParticleModuleSpawn.h"
#include "Particles/Modules/ParticleModuleTypeDataMesh.h"
#include "Asset/AssetManager.h"
#include "Render/Mesh.h"
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

UParticleLODLevel* FParticleEditorContext::GetLODLevel(const int32 EmitterIndex) const
{
	const UParticleSystem* Template = GetTemplate();
	if (!Template || EmitterIndex < 0 || EmitterIndex >= Template->Emitters.Num())
		return nullptr;
	const UParticleEmitter* Emitter = Template->Emitters[EmitterIndex];
	if (!Emitter || Emitter->LODLevels.IsEmpty())
		return nullptr;
	return Emitter->LODLevels[std::clamp(CurrentLODIndex, 0, Emitter->LODLevels.Num() - 1)];
}

bool FParticleEditorContext::HasModuleOfClass(const int32 EmitterIndex, const UClass* ModuleClass) const
{
	const UParticleLODLevel* LODLevel = GetLODLevel(EmitterIndex);
	if (!LODLevel || !ModuleClass)
		return false;
	for (UParticleModule* Module : LODLevel->Modules)
	{
		if (Module && Module->GetClass() == ModuleClass)
			return true;
	}
	return false;
}

bool FParticleEditorContext::AddModule(const int32 EmitterIndex, UClass* ModuleClass)
{
	UParticleLODLevel* LODLevel = GetLODLevel(EmitterIndex);
	if (!LODLevel || !ModuleClass || HasModuleOfClass(EmitterIndex, ModuleClass))
		return false;

	// 소유자는 LOD 레벨이다. (기본 에셋을 만들 때와 같은 Outer 규칙)
	UParticleModule* NewModule = NewObject<UParticleModule>(LODLevel, ModuleClass);
	if (!NewModule)
		return false;

	LODLevel->Modules.Add(NewModule);
	ApplyEmitterChange(EmitterIndex);
	SelectedEmitterIndex = EmitterIndex;
	SelectedModule = NewModule;
	return true;
}

bool FParticleEditorContext::CanDeleteModule(const UParticleModule* Module)
{
	return Module && !Cast<UParticleModuleRequired>(const_cast<UParticleModule*>(Module))
		&& !Cast<UParticleModuleSpawn>(const_cast<UParticleModule*>(Module));
}

bool FParticleEditorContext::DeleteModule(const int32 EmitterIndex, UParticleModule* Module)
{
	UParticleLODLevel* LODLevel = GetLODLevel(EmitterIndex);
	if (!LODLevel || !CanDeleteModule(Module))
		return false;

	if (LODLevel->TypeDataModule == Module)
	{
		LODLevel->TypeDataModule = nullptr;
	}
	else
	{
		const int32 Index = LODLevel->Modules.Find(Module);
		if (Index == INDEX_NONE)
			return false;
		LODLevel->Modules.RemoveAt(Index);
	}

	// 이미터 캐시(Spawn/Update 목록·Payload 오프셋)와 실행 중인 인스턴스가 이 모듈을 가리키고 있다.
	// 먼저 캐시와 인스턴스를 새로 만든 뒤에 지워야 해제된 모듈을 가리키는 순간이 없다.
	ApplyEmitterChange(EmitterIndex);
	if (SelectedModule == Module)
		SelectedModule = nullptr;
	delete Module;
	return true;
}

bool FParticleEditorContext::AddMeshTypeData(const int32 EmitterIndex)
{
	UParticleLODLevel* LODLevel = GetLODLevel(EmitterIndex);
	if (!LODLevel || LODLevel->TypeDataModule)
		return false;

	UParticleModuleTypeDataMesh* MeshData = NewObject<UParticleModuleTypeDataMesh>(LODLevel);
	if (!MeshData)
		return false;
	// 메시가 없으면 아무것도 그려지지 않으므로 엔진 기본 도형을 넣어 둔다. 디테일 패널에서 바꿀 수 있게 할 예정이다.
	MeshData->Mesh = UAssetManager::GetAssetByPath<UStaticMesh>("Cube");

	LODLevel->TypeDataModule = MeshData;
	ApplyEmitterChange(EmitterIndex);
	SelectedEmitterIndex = EmitterIndex;
	SelectedModule = MeshData;
	return true;
}

void FParticleEditorContext::ApplyEmitterChange(const int32 EmitterIndex)
{
	// 이 에셋을 쓰는 컴포넌트가 하나도 없어도 이미터 캐시는 최신이어야 한다.
	// (캐시가 지운 모듈을 가리킨 채로 남지 않도록 RestartLevelInstances와 별도로 다시 만든다.)
	if (UParticleSystem* Template = GetTemplate())
	{
		if (EmitterIndex >= 0 && EmitterIndex < Template->Emitters.Num() && Template->Emitters[EmitterIndex])
			Template->Emitters[EmitterIndex]->CacheEmitterModuleInfo();
	}
	RestartLevelInstances();
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

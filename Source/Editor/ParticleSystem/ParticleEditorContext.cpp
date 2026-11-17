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
	EditedAsset = nullptr;
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

	// 저장 결과 문구는 몇 초만 보여 준다. 일시정지 중에도 사라지도록 실제 시간으로 센다.
	if (SaveStatusTimer > 0.0f)
	{
		SaveStatusTimer -= DeltaTime;
		if (SaveStatusTimer <= 0.0f)
			SaveStatus.clear();
	}
}

void FParticleEditorContext::RestartSimulation()
{
	if (!PreviewScene)
		return;

	// 삭제는 프리뷰 월드의 다음 Tick에서 처리된다. 새 액터는 빈 이미터로 시작한다.
	if (PreviewActor)
		PreviewScene->DestroyActor(PreviewActor);
	PreviewActor = PreviewScene->SpawnActor<AParticleSystemActor>();

	// 새 액터는 기본 에셋으로 시작하므로, 사용자가 고른 에셋이 있으면 다시 씌운다.
	if (EditedAsset && PreviewActor && PreviewActor->ParticleSystemComponent)
		PreviewActor->ParticleSystemComponent->SetTemplate(EditedAsset);
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
	// 모듈을 더하거나 지우거나 값을 고치는 편집은 모두 여기를 지난다.
	MarkDirty();

	// 이 에셋을 쓰는 컴포넌트가 하나도 없어도 이미터 캐시는 최신이어야 한다.
	// (캐시가 지운 모듈을 가리킨 채로 남지 않도록 RestartLevelInstances와 별도로 다시 만든다.)
	if (UParticleSystem* Template = GetTemplate())
	{
		if (EmitterIndex >= 0 && EmitterIndex < Template->Emitters.Num() && Template->Emitters[EmitterIndex])
			Template->Emitters[EmitterIndex]->CacheEmitterModuleInfo();
	}
	RestartLevelInstances();
}

namespace
{
	constexpr const char* ParticleAssetFolder = "Assets/Particles/";
	constexpr const char* ParticleAssetExtension = ".particle";
	constexpr float SaveStatusSeconds = 4.0f;

	bool EndsWithIgnoreCase(const FString& Text, const char* Suffix)
	{
		const size_t SuffixLength = std::strlen(Suffix);
		if (Text.size() < SuffixLength)
			return false;
		for (size_t Index = 0; Index < SuffixLength; ++Index)
		{
			const char A = static_cast<char>(std::tolower(static_cast<unsigned char>(Text[Text.size() - SuffixLength + Index])));
			const char B = static_cast<char>(std::tolower(static_cast<unsigned char>(Suffix[Index])));
			if (A != B)
				return false;
		}
		return true;
	}
}

bool FParticleEditorContext::HasAssetFile() const
{
	const UParticleSystem* Template = GetTemplate();
	return Template && EndsWithIgnoreCase(Template->GetPath(), ParticleAssetExtension);
}

bool FParticleEditorContext::IsDirty() const
{
	const UParticleSystem* Template = GetTemplate();
	return Template && DirtyAssets.Find(Template) != INDEX_NONE;
}

void FParticleEditorContext::MarkDirty()
{
	const UParticleSystem* Template = GetTemplate();
	if (Template && DirtyAssets.Find(Template) == INDEX_NONE)
		DirtyAssets.Add(Template);
}

bool FParticleEditorContext::Save()
{
	UParticleSystem* Template = GetTemplate();
	if (!Template || !HasAssetFile())
		return false;

	// 에셋 키는 Assets 기준 상대 경로이므로 앞에 Assets/를 붙이면 파일 경로가 된다.
	FString Error;
	if (!UAssetManager::Get().SaveParticleSystem(FString("Assets/") + Template->GetPath(), Template, Error))
	{
		SetSaveStatus("Save failed: " + Error, true);
		return false;
	}

	const int32 DirtyIndex = DirtyAssets.Find(Template);
	if (DirtyIndex != INDEX_NONE)
		DirtyAssets.RemoveAt(DirtyIndex);
	SetSaveStatus("Saved " + Template->GetPath(), false);
	return true;
}

bool FParticleEditorContext::SaveAs(const FString& AssetName, FString& OutError)
{
	OutError.clear();
	UParticleSystem* Template = GetTemplate();
	if (!Template)
	{
		OutError = "There is no particle system to save.";
		return false;
	}

	// 폴더 구분자나 확장자가 섞이면 Assets/Particles 밖에 쓰이거나 이름이 틀어진다. (콘텐츠 드로어와 같은 규칙)
	if (AssetName.empty() || AssetName.find_first_of("<>:\"/\\|?*.") != FString::npos || AssetName.back() == ' ')
	{
		OutError = "Enter a file name without an extension or reserved characters.";
		return false;
	}

	const FString Path = FString(ParticleAssetFolder) + AssetName + ParticleAssetExtension;
	UAssetManager& AssetManager = UAssetManager::Get();
	if (!AssetManager.SaveParticleSystem(Path, Template, OutError, true))
		return false;

	// 저장한 파일을 읽어 새 에셋 객체로 바꾼다. 원래 에셋 객체(기본 에셋 등)는 그대로 남는다.
	UParticleSystem* Saved = AssetManager.LoadParticleSystem(Path, OutError);
	if (!Saved)
		return false;

	ApplyPreviewTemplate(Saved);
	SetSaveStatus("Saved " + Saved->GetPath(), false);
	return true;
}

void FParticleEditorContext::OpenAsset(UParticleSystem* Asset)
{
	if (Asset && Asset != GetTemplate())
		ApplyPreviewTemplate(Asset);
}

FString FParticleEditorContext::GetAssetLabel() const
{
	const UParticleSystem* Template = GetTemplate();
	return Template ? Template->GetPath() : FString();
}

void FParticleEditorContext::ApplyPreviewTemplate(UParticleSystem* Asset)
{
	EditedAsset = Asset;
	// 선택은 이전 에셋의 이미터·모듈을 가리키므로 함께 비운다.
	SelectedEmitterIndex = INDEX_NONE;
	SelectedModule = nullptr;
	CurrentLODIndex = 0;

	if (UParticleSystemComponent* Component = GetPreviewComponent())
		Component->SetTemplate(Asset);
}

void FParticleEditorContext::SetSaveStatus(const FString& Message, const bool bError)
{
	SaveStatus = Message;
	bSaveStatusError = bError;
	SaveStatusTimer = SaveStatusSeconds;
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

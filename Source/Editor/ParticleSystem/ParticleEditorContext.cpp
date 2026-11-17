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
#include "Particles/Modules/ParticleModuleLifetime.h"
#include "Particles/Modules/ParticleModuleSize.h"
#include "Particles/Modules/ParticleModuleVelocity.h"
#include "Particles/Modules/ParticleModuleColor.h"
#include "Particles/Modules/ParticleModuleTypeDataBase.h"
#include "Particles/Modules/ParticleModuleTypeDataBeam.h"
#include "Particles/Modules/ParticleModuleTypeDataRibbon.h"
#include "Particles/Modules/ParticleModuleBeamSource.h"
#include "Particles/Modules/ParticleModuleBeamTarget.h"
#include "Particles/Modules/ParticleModuleBeamNoise.h"
#include "UObject/Casts.h"
#include "Asset/AssetManager.h"
#include "Render/Mesh.h"
#include "UObject/UObjectIterator.h"

#include <algorithm>

FParticleEditorContext::~FParticleEditorContext()
{
	Release();
}

bool FParticleEditorContext::Init(const FName WorldName)
{
	PreviewScene = MakeUnique<FPreviewScene>(WorldName);
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
	UParticleSystemComponent* ParticleSystemComponent = GetPreviewComponent();
	if (ParticleSystemComponent)
	{
		ParticleSystemComponent->bOverrideLODMethod = true;
		ParticleSystemComponent->LODMethod = EParticleSystemLODMethod::DirectSet;
		ParticleSystemComponent->SetLODLevel(CurrentLODIndex);
	}
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
	if (!CanAddModule(EmitterIndex, ModuleClass))
	{
		return false;
	}

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

bool FParticleEditorContext::CanAddModule(
    const int32 EmitterIndex,
    UClass* ModuleClass) const
{
	const UParticleLODLevel* LODLevel =
	    GetLODLevel(EmitterIndex);

	if (!LODLevel ||
	    !ModuleClass ||
	    !ModuleClass->IsChildOf(UParticleModule::StaticClass()) ||
	    !ModuleClass->CanCreateInstance() ||
	    HasModuleOfClass(EmitterIndex, ModuleClass))
	{
		return false;
	}

	// 별도 필드나 별도 생성 경로로 관리하는 모듈.
	if (ModuleClass->IsChildOf(
	        UParticleModuleTypeDataBase::StaticClass()) ||
	    ModuleClass->IsChildOf(
	        UParticleModuleRequired::StaticClass()) ||
	    ModuleClass->IsChildOf(
	        UParticleModuleSpawn::StaticClass()))
	{
		return false;
	}

	const bool bBeamModule =
	    ModuleClass->IsChildOf(
	        UParticleModuleBeamSource::StaticClass()) ||
	    ModuleClass->IsChildOf(
	        UParticleModuleBeamTarget::StaticClass()) ||
	    ModuleClass->IsChildOf(
	        UParticleModuleBeamNoise::StaticClass());

	if (bBeamModule)
	{
		return Cast<UParticleModuleTypeDataBeam>(
		           LODLevel->TypeDataModule) != nullptr;
	}

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

	if (LODLevel->TypeDataModule == Module &&
	    Cast<UParticleModuleTypeDataBeam>(Module))
	{
		for (UParticleModule* Existing : LODLevel->Modules)
		{
			if (Cast<UParticleModuleBeamSource>(Existing) ||
			    Cast<UParticleModuleBeamTarget>(Existing) ||
			    Cast<UParticleModuleBeamNoise>(Existing))
			{
				return false;
			}
		}
	}

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

bool FParticleEditorContext::AddBeamTypeData(
    const int32 EmitterIndex)
{
	UParticleLODLevel* LODLevel = GetLODLevel(EmitterIndex);

	if (!LODLevel ||
	    !LODLevel->RequiredModule ||
	    LODLevel->TypeDataModule)
	{
		return false;
	}

	UParticleModuleTypeDataBeam* BeamData =
	    NewObject<UParticleModuleTypeDataBeam>(LODLevel);

	if (!BeamData)
	{
		return false;
	}

	BeamData->MaxBeamCount = 1;
	BeamData->SegmentCount = 32;
	BeamData->TextureTileDistance = 0.0f;

	LODLevel->TypeDataModule = BeamData;

	// 처음 전환한 직후에도 Source/Target을 편집할 수 있게 구성.
	// Context.AddModule()을 사용하면 중간마다 재시작하므로,
	// 여기서는 직접 추가하고 마지막에 한 번만 재시작한다.
	if (!HasModuleOfClass(
	        EmitterIndex,
	        UParticleModuleBeamSource::StaticClass()))
	{
		auto* Source =
		    NewObject<UParticleModuleBeamSource>(LODLevel);

		Source->SourcePosition = FVector(0.0f, 0.0f, 0.0f);
		Source->bLockOnSpawn = false;

		LODLevel->Modules.Add(Source);
	}

	if (!HasModuleOfClass(
	        EmitterIndex,
	        UParticleModuleBeamTarget::StaticClass()))
	{
		auto* Target =
		    NewObject<UParticleModuleBeamTarget>(LODLevel);

		Target->TargetPosition = FVector(5.0f, 0.0f, 0.0f);
		Target->bLockOnSpawn = false;

		LODLevel->Modules.Add(Target);
	}

	ApplyEmitterChange(EmitterIndex);

	SelectedEmitterIndex = EmitterIndex;
	SelectedModule = BeamData;
	return true;
}

bool FParticleEditorContext::AddRibbonTypeData(
    const int32 EmitterIndex)
{
	UParticleLODLevel* LODLevel = GetLODLevel(EmitterIndex);

	if (!LODLevel ||
	    !LODLevel->RequiredModule ||
	    LODLevel->TypeDataModule)
	{
		return false;
	}

	auto* RibbonData =
	    NewObject<UParticleModuleTypeDataRibbon>(LODLevel);

	if (!RibbonData)
	{
		return false;
	}

	RibbonData->MaxTrailCount = 1;
	RibbonData->MaxPointsPerTrail = 128;
	RibbonData->MinSpawnDistance = 0.1f;
	RibbonData->TextureTileDistance = 0.0f;
	RibbonData->bClipSourceSegment = false;

	// 현재 Ribbon 구현의 필수 조건.
	LODLevel->RequiredModule->bUseLocalSpace = false;

	LODLevel->TypeDataModule = RibbonData;

	ApplyEmitterChange(EmitterIndex);

	SelectedEmitterIndex = EmitterIndex;
	SelectedModule = RibbonData;
	return true;
}

int32 FParticleEditorContext::AddSpriteEmitter()
{
	UParticleSystem* Template = GetTemplate();
	if (!Template)
		return INDEX_NONE;

	// 소유 관계는 기본 에셋을 만들 때와 같다. 시스템 → 이미터 → LOD 레벨 → 모듈 순으로 Outer가 된다.
	UParticleEmitter* Emitter = NewObject<UParticleEmitter>(Template);
	UParticleLODLevel* LODLevel = NewObject<UParticleLODLevel>(Emitter);
	LODLevel->Level = 0;

	// Material이 nullptr이면 인스턴스가 DefaultParticleMaterial로 그린다.
	UParticleModuleRequired* Required = NewObject<UParticleModuleRequired>(LODLevel);
	Required->MaxActiveParticles = 100;

	UParticleModuleSpawn* Spawn = NewObject<UParticleModuleSpawn>(LODLevel);
	Spawn->SpawnRate = 20.0f;

	UParticleModuleLifetime* Lifetime = NewObject<UParticleModuleLifetime>(LODLevel);
	Lifetime->Lifetime = FParticleFloatRange(1.0f, 1.0f);

	UParticleModuleSize* Size = NewObject<UParticleModuleSize>(LODLevel);
	Size->StartSize = FParticleVectorRange(FVector(0.5f, 0.5f, 0.5f));

	// 기존 이미터와 겹쳐도 구분되도록 위쪽(Z)으로 곧게 올라가게 한다.
	UParticleModuleVelocity* Velocity = NewObject<UParticleModuleVelocity>(LODLevel);
	Velocity->StartVelocity = FParticleVectorRange(FVector(-0.5f, -0.5f, 2.0f), FVector(0.5f, 0.5f, 3.0f));

	// Cascade 기본 이미터의 Color Over Life처럼 흰색에서 투명으로 사라지게 한다.
	UParticleModuleColor* Color = NewObject<UParticleModuleColor>(LODLevel);
	Color->StartColor = FParticleColorRange(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
	Color->bUseColorOverLife = true;
	Color->SetFadeOut();

	LODLevel->RequiredModule = Required;
	LODLevel->Modules.Add(Spawn);
	LODLevel->Modules.Add(Lifetime);
	LODLevel->Modules.Add(Size);
	LODLevel->Modules.Add(Velocity);
	LODLevel->Modules.Add(Color);

	// 지금은 모든 에셋이 LOD 0 하나뿐이다. LOD 편집이 생기면 다른 이미터와 LOD 수를 맞춰야 한다.
	Emitter->LODLevels.Add(LODLevel);
	Template->Emitters.Add(Emitter);

	// 인스턴스 수가 Emitters와 달라졌으므로 캐시를 만들고 이 에셋을 쓰는 컴포넌트를 모두 다시 시작한다.
	const int32 NewIndex = Template->Emitters.Num() - 1;
	ApplyEmitterChange(NewIndex);
	SelectedEmitterIndex = NewIndex;
	SelectedModule = nullptr;
	return NewIndex;
}

bool FParticleEditorContext::CanDeleteEmitter() const
{
	const UParticleSystem* Template = GetTemplate();
	return Template && Template->Emitters.Num() > 1;
}

bool FParticleEditorContext::DeleteEmitter(const int32 EmitterIndex)
{
	UParticleSystem* Template = GetTemplate();
	if (!CanDeleteEmitter() || EmitterIndex < 0 || EmitterIndex >= Template->Emitters.Num())
		return false;

	UParticleEmitter* Emitter = Template->Emitters[EmitterIndex];
	Template->Emitters.RemoveAt(EmitterIndex);

	// 이미터 인스턴스가 지울 이미터(SpriteTemplate)와 그 모듈을 가리키고 있다.
	// 먼저 이 에셋을 쓰는 모든 컴포넌트의 인스턴스를 남은 이미터 기준으로 다시 만든 뒤에 지운다.
	// 월드 밖 컴포넌트는 다시 시작하지 않으므로 인스턴스만 비워 옛 이미터를 가리키지 않게 한다.
	for (TObjectIterator<UParticleSystemComponent> It; It; ++It)
	{
		UParticleSystemComponent* Component = *It;
		if (!Component || Component->Template != Template)
			continue;
		const AActor* Owner = Component->GetOwner();
		if (Owner && Owner->GetWorld())
			Component->ActivateSystem(true);
		else
			Component->ResetParticles();
	}

	// 지운 이미터 뒤의 번호가 하나씩 당겨지므로 선택을 비운다. 디테일 패널은 시스템 전체를 보여 준다.
	SelectedEmitterIndex = INDEX_NONE;
	SelectedModule = nullptr;

	// 소유 트리를 아래에서부터 지운다. (모듈 → LOD 레벨 → 이미터)
	if (Emitter)
	{
		for (UParticleLODLevel* LODLevel : Emitter->LODLevels)
		{
			if (!LODLevel)
				continue;
			for (UParticleModule* Module : LODLevel->Modules)
				delete Module;
			delete LODLevel->RequiredModule;
			delete LODLevel->TypeDataModule;
			delete LODLevel;
		}
		delete Emitter;
	}
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

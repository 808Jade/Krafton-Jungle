#pragma once
#include "PreviewScene.h"

class AParticleSystemActor;
class UParticleSystemComponent;
class UParticleSystem;
class UParticleModule;
class UParticleLODLevel;
class UClass;
struct FParticleEmitterInstance;

// 파티클 시스템 에디터의 하위 패널들이 공유하는 상태. 패널끼리는 서로를 모르고 이 객체로만 통신한다.
// (예: 이미터 패널이 선택을 바꾸면 디테일 패널이 다음 프레임에 그 선택을 읽어 그린다.)
class FParticleEditorContext
{
public:
	FParticleEditorContext() = default;
	~FParticleEditorContext();
	FParticleEditorContext(const FParticleEditorContext&) = delete;
	FParticleEditorContext& operator=(const FParticleEditorContext&) = delete;

	// WorldName: 탭마다 다른 이름을 주어 프리뷰 월드끼리 구분한다.
	bool Init(FName WorldName = FName("ParticlePreviewWorld"));
	// 프리뷰 월드를 해제한다. 전역 UObject 일괄 삭제 전에 호스트가 호출한다. 여러 번 호출해도 안전하다.
	void Release();
	// 일시정지 중에도 Transform 갱신과 삭제 대기 처리를 위해 0초로 Tick한다.
	void Tick(float DeltaTime);
	// 프리뷰 액터를 지우고 다시 만들어 시뮬레이션을 처음부터 시작한다.
	void RestartSimulation();

	// --- 저장 / 열기 ---
	// 편집 중인 에셋이 있으면 저장할 수 있다. 파일이 없는 에셋(코드로 만든 기본 에셋)은 Save가 아니라 SaveAs로 저장한다.
	bool CanSave() const { return GetTemplate() != nullptr; }
	// 편집 중인 에셋이 .particle 파일에서 온 것이면 true. 이때만 Save가 파일에 덮어쓸 수 있다.
	bool HasAssetFile() const;
	// 마지막 저장 뒤에 이 에셋을 고쳤으면 true. 에셋마다 따로 센다.
	bool IsDirty() const;
	// 에셋 값을 고쳤을 때 부른다. 모듈 편집은 ApplyEmitterChange가 대신 부르므로, 에셋 자체 값(바운드 등)을 고칠 때만 직접 부른다.
	void MarkDirty();
	// 에셋 파일에 덮어쓴다. HasAssetFile()일 때만 성공한다. 결과는 GetSaveStatus()에 한동안 남는다.
	bool Save();
	// Assets/Particles/<AssetName>.particle에 새로 저장하고, 그 파일을 불러와 프리뷰의 에셋으로 바꾼다.
	// 이미 있는 이름이거나 이름이 올바르지 않으면 false이고 OutError에 이유를 담는다.
	bool SaveAs(const FString& AssetName, FString& OutError);
	// 등록된 파티클 시스템 에셋(저장한 파일 + 기본 에셋)을 프리뷰의 에셋으로 바꾼다.
	void OpenAsset(UParticleSystem* Asset);
	// 툴바·메뉴에 보일 에셋 이름. 에셋이 없으면 빈 문자열이다.
	FString GetAssetLabel() const;
	// 저장 결과 문구. 몇 초가 지나면 빈 문자열이 된다. 실패 문구인지는 IsSaveStatusError()로 안다.
	const FString& GetSaveStatus() const { return SaveStatus; }
	bool IsSaveStatusError() const { return bSaveStatusError; }

	// 같은 Template을 쓰는 모든 컴포넌트(프리뷰 + 레벨·PIE 월드에 놓인 것)를 처음부터 다시 시작한다.
	// 에셋을 고친 뒤 레벨 쪽에도 반영할 때 쓴다. 다시 시작한 컴포넌트 수를 돌려준다.
	int32 RestartLevelInstances();

	// --- LOD ---
	// 파티클 시스템의 LOD 수(LODDistances 칸 수). 에셋이 없거나 LOD가 없으면 1이다.
	int32 GetLODCount() const;
	// CurrentLODIndex를 0 ~ GetLODCount()-1 안으로 맞춰 설정한다.
	void SetCurrentLOD(int32 NewLODIndex);
	bool CanEditModuleStructure() const { return CurrentLODIndex == 0; }
	bool CanAddLODBefore() const { return GetTemplate() && CurrentLODIndex > 0; }
	bool CanAddLODAfter() const { return GetTemplate() != nullptr; }
	bool CanDeleteLOD() const { return GetTemplate() && CurrentLODIndex > 0; }
	bool AddLOD(bool bAfterCurrent);
	bool DeleteCurrentLOD();
	bool CanRegenerateLowestLOD() const { return GetTemplate() != nullptr; }
	bool RegenerateLowestLOD(bool bDuplicateHighest);
	bool NormalizeLODSetup();

	// --- 에셋 편집 (이미터 패널 우클릭 메뉴) ---
	// 편집 대상은 EmitterIndex 이미터의 현재 LOD 레벨이다. 편집이 끝나면 이미터 캐시를 다시 만들고
	// 같은 Template을 쓰는 컴포넌트를 모두 다시 시작한다. (모듈 Payload 배치가 바뀌기 때문)
	UParticleLODLevel* GetLODLevel(int32 EmitterIndex) const;
	bool HasModuleOfClass(int32 EmitterIndex, const UClass* ModuleClass) const;
	// ModuleClass 모듈을 새로 만들어 모듈 목록 끝에 붙이고 선택한다. 같은 종류가 이미 있으면 실패한다.
	bool AddModule(int32 EmitterIndex, UClass* ModuleClass);
	// Required·Spawn은 이미터에 꼭 하나씩 있어야 하므로 지울 수 없다. (언리얼도 같다)
	bool CanAddModule(int32 EmitterIndex, UClass* ModuleClass) const;
	static bool CanDeleteModule(const UParticleModule* Module);
	// 모듈 목록 또는 TypeData에서 Module을 빼고 삭제한다.
	bool DeleteModule(int32 EmitterIndex, UParticleModule* Module);
	// 메시 TypeData를 붙여 메시 이미터로 바꾼다. 기본 메시는 엔진의 "Cube"다. TypeData가 이미 있으면 실패한다.
	bool AddMeshTypeData(int32 EmitterIndex);
	bool AddBeamTypeData(int32 EmitterIndex);
	bool AddRibbonTypeData(int32 EmitterIndex);
	// Cascade의 "New Particle Sprite Emitter". 기본 모듈 구성(Required·Spawn·Lifetime·Initial Size·
	// Initial Velocity·Initial Color)을 가진 스프라이트 이미터를 Emitters 끝에 붙이고 선택한다.
	// 새 이미터의 인덱스를 돌려준다. 에셋이 없으면 INDEX_NONE이다.
	int32 AddSpriteEmitter();
	// 이미터가 하나뿐이면 지울 수 없다. (빈 파티클 시스템은 아무것도 재생하지 않는다)
	bool CanDeleteEmitter() const;
	// EmitterIndex 이미터를 에셋에서 빼고, 그 이미터가 소유한 LOD 레벨·모듈까지 함께 삭제한다.
	bool DeleteEmitter(int32 EmitterIndex);
	// 디테일 패널에서 모듈 값을 고친 뒤 부른다. NeedsUpdate·MaxActiveParticles·Mesh처럼
	// 이미터 캐시에 들어가는 값이 바뀌었을 수 있으므로 캐시를 다시 만들고 컴포넌트를 다시 시작한다.
	void NotifyModuleEdited(int32 EmitterIndex) { ApplyEmitterChange(EmitterIndex); }

	UWorld* GetPreviewWorld() const { return PreviewScene ? PreviewScene->GetWorld() : nullptr; }
	UParticleSystemComponent* GetPreviewComponent() const;
	// 프리뷰 컴포넌트가 재생 중인 파티클 시스템 에셋. 이미터·모듈 구성은 여기서 읽는다.
	UParticleSystem* GetTemplate() const;
	// 선택 인덱스가 현재 인스턴스 목록 범위를 벗어나면 nullptr이다. (재시작 직후 등)
	FParticleEmitterInstance* GetSelectedEmitterInstance() const;

	// --- 패널이 읽고 쓰는 공유 상태 ---
	int32 SelectedEmitterIndex = INDEX_NONE;
	// 참조(집약): 이미터 패널에서 고른 모듈. 소유자는 에셋의 UParticleLODLevel이다.
	// nullptr이면 이미터(또는 시스템) 자체가 선택된 상태다. Template이 바뀌면 함께 비운다.
	UParticleModule* SelectedModule = nullptr;
	bool bSimulationPaused = false;
	// 툴바에 보이는 현재 LOD 번호. 이미터 패널이 이 번호의 LOD 레벨 모듈을 보여 준다.
	// 바꿀 때는 SetCurrentLOD로 범위를 맞춘다. 프리뷰 컴포넌트도 이 LOD로 재생한다.
	int32 CurrentLODIndex = 0;

private:
	// 에셋 구성이 바뀐 이미터의 모듈 캐시를 다시 만들고, 이 에셋을 쓰는 컴포넌트를 다시 시작한다.
	void ApplyEmitterChange(int32 EmitterIndex);
	void RebuildAllEmitters();
	// 프리뷰 컴포넌트가 재생할 에셋을 바꾸고 선택 상태를 비운다. nullptr이면 기본 에셋이다.
	void ApplyPreviewTemplate(UParticleSystem* Asset);
	void SetSaveStatus(const FString& Message, bool bError);

	// 참조(집약): 사용자가 고른 에셋. nullptr이면 액터가 기본으로 쓰는 에셋이다.
	// RestartSimulation이 액터를 새로 만들 때 이 에셋을 다시 씌우는 데 쓴다.
	UParticleSystem* EditedAsset = nullptr;
	// 참조(집약): 저장하지 않은 수정이 있는 에셋. 에셋은 UAssetManager가 소유한다.
	TArray<const UParticleSystem*> DirtyAssets;
	FString SaveStatus;
	bool bSaveStatusError = false;
	float SaveStatusTimer = 0.0f;

	// 소유(합성): 이 에디터만 쓰는 독립 월드. 에디터와 수명이 같다.
	TUniquePtr<FPreviewScene> PreviewScene;
	// 참조(집약): 프리뷰 월드가 소유하고 지우는 액터. 여기서는 가리키기만 한다.
	AParticleSystemActor* PreviewActor = nullptr;
};

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

	bool Init();
	// 프리뷰 월드를 해제한다. 전역 UObject 일괄 삭제 전에 호스트가 호출한다. 여러 번 호출해도 안전하다.
	void Release();
	// 일시정지 중에도 Transform 갱신과 삭제 대기 처리를 위해 0초로 Tick한다.
	void Tick(float DeltaTime);
	// 프리뷰 액터를 지우고 다시 만들어 시뮬레이션을 처음부터 시작한다.
	void RestartSimulation();

	// 편집 중인 파티클 시스템 에셋을 저장한다. 에셋 모델(UParticleSystem)이 아직 없어 지금은 항상 저장할 대상이 없다.
	// TODO: 에셋을 열면 그 에셋이 있을 때 true, Save에서 에셋을 직렬화해 파일로 쓴다.
	bool CanSave() const { return false; }
	bool Save() { return false; }

	// 같은 Template을 쓰는 모든 컴포넌트(프리뷰 + 레벨·PIE 월드에 놓인 것)를 처음부터 다시 시작한다.
	// 에셋을 고친 뒤 레벨 쪽에도 반영할 때 쓴다. 다시 시작한 컴포넌트 수를 돌려준다.
	int32 RestartLevelInstances();

	// --- LOD ---
	// 이미터들 가운데 가장 많은 LOD 레벨 수. 에셋이 없거나 LOD가 없으면 1이다.
	int32 GetLODCount() const;
	// CurrentLODIndex를 0 ~ GetLODCount()-1 안으로 맞춰 설정한다.
	void SetCurrentLOD(int32 NewLODIndex);

	// --- 에셋 편집 (이미터 패널 우클릭 메뉴) ---
	// 편집 대상은 EmitterIndex 이미터의 현재 LOD 레벨이다. 편집이 끝나면 이미터 캐시를 다시 만들고
	// 같은 Template을 쓰는 컴포넌트를 모두 다시 시작한다. (모듈 Payload 배치가 바뀌기 때문)
	UParticleLODLevel* GetLODLevel(int32 EmitterIndex) const;
	bool HasModuleOfClass(int32 EmitterIndex, const UClass* ModuleClass) const;
	// ModuleClass 모듈을 새로 만들어 모듈 목록 끝에 붙이고 선택한다. 같은 종류가 이미 있으면 실패한다.
	bool AddModule(int32 EmitterIndex, UClass* ModuleClass);
	// Required·Spawn은 이미터에 꼭 하나씩 있어야 하므로 지울 수 없다. (언리얼도 같다)
	static bool CanDeleteModule(const UParticleModule* Module);
	// 모듈 목록 또는 TypeData에서 Module을 빼고 삭제한다.
	bool DeleteModule(int32 EmitterIndex, UParticleModule* Module);
	// 메시 TypeData를 붙여 메시 이미터로 바꾼다. 기본 메시는 엔진의 "Cube"다. TypeData가 이미 있으면 실패한다.
	bool AddMeshTypeData(int32 EmitterIndex);

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
	// 바꿀 때는 SetCurrentLOD로 범위를 맞춘다. 런타임 시뮬레이션은 아직 LOD 0만 쓴다.
	int32 CurrentLODIndex = 0;

private:
	// 에셋 구성이 바뀐 이미터의 모듈 캐시를 다시 만들고, 이 에셋을 쓰는 컴포넌트를 다시 시작한다.
	void ApplyEmitterChange(int32 EmitterIndex);

	// 소유(합성): 이 에디터만 쓰는 독립 월드. 에디터와 수명이 같다.
	TUniquePtr<FPreviewScene> PreviewScene;
	// 참조(집약): 프리뷰 월드가 소유하고 지우는 액터. 여기서는 가리키기만 한다.
	AParticleSystemActor* PreviewActor = nullptr;
};

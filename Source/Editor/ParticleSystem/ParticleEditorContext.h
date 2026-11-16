#pragma once
#include "PreviewScene.h"

class AParticleSystemActor;
class UParticleSystemComponent;
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

	UWorld* GetPreviewWorld() const { return PreviewScene ? PreviewScene->GetWorld() : nullptr; }
	UParticleSystemComponent* GetPreviewComponent() const;
	// 선택 인덱스가 현재 인스턴스 목록 범위를 벗어나면 nullptr이다. (재시작 직후 등)
	FParticleEmitterInstance* GetSelectedEmitterInstance() const;

	// --- 패널이 읽고 쓰는 공유 상태 ---
	int32 SelectedEmitterIndex = INDEX_NONE;
	bool bSimulationPaused = false;

private:
	// 소유(합성): 이 에디터만 쓰는 독립 월드. 에디터와 수명이 같다.
	TUniquePtr<FPreviewScene> PreviewScene;
	// 참조(집약): 프리뷰 월드가 소유하고 지우는 액터. 여기서는 가리키기만 한다.
	AParticleSystemActor* PreviewActor = nullptr;
};

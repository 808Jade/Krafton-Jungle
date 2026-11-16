#pragma once

#include "Engine/World.h"

// 애셋 에디터(파티클 에디터 등) 프리뷰용 독립 월드를 소유하고, 그 수명과 틱을 관리한다.
// 에디터 월드와 분리된 EWorldType::EditorPreview 월드라서 컴포넌트가 정식 경로
// (월드 등록 → 틱 → SceneProxy)를 그대로 탄다. 렌더링은 소유자가 GetWorld()를 렌더러에 넘겨 처리한다.
class FPreviewScene
{
public:
	explicit FPreviewScene(FName InWorldName = NAME_None);
	virtual ~FPreviewScene();
	FPreviewScene(const FPreviewScene&) = delete;
	FPreviewScene& operator=(const FPreviewScene&) = delete;

	// 프리뷰 월드를 한 프레임 진행한다. 스폰된 액터의 BeginPlay와 컴포넌트 Tick이 여기서 실행된다.
	void Tick(float DeltaTime);

	// 프리뷰 월드와 그 안의 액터를 모두 해제한다. 여러 번 호출해도 안전하다.
	// 소유자가 전역 UObject 일괄 삭제(DestroyAllObjectsExceptEngine)보다 오래 살면 그 전에 호출해야 한다.
	void Release();

	template <class T>
	T* SpawnActor(FName InName = NAME_None, const FTransform* Transform = nullptr)
	{
		return PreviewWorld ? PreviewWorld->SpawnActor<T>(InName, Transform) : nullptr;
	}

	// 액터를 즉시 지우지 않고 월드의 다음 Tick에서 정리되도록 요청한다.
	bool DestroyActor(AActor* Actor);

	UWorld* GetWorld() const { return PreviewWorld; }

private:
	// 소유: 이 객체가 생성하고 Release에서 직접 delete한다.
	UWorld* PreviewWorld = nullptr;
};

#include "EnginePCH.h"
#include "PreviewScene.h"
#include "Camera/CameraComponent.h"

FPreviewScene::FPreviewScene(FName InWorldName)
{
	// 엔진 WorldList에 넣지 않는다. 넣으면 에디터의 활성 월드 전환·PIE 로직이 프리뷰 월드를 대상으로 삼는다.
	PreviewWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false, InWorldName);
	if (!PreviewWorld)
		return;
	PreviewWorld->SetFlags(EObjectFlags::RF_Transient);

	// 월드 기본 카메라는 Tick에서 WASD·마우스·휠을 직접 읽는다. 프리뷰는 소유자의 View 카메라로 그리므로
	// 이 카메라가 에디터 입력(특히 프레임당 한 번만 읽는 휠 값)을 가로채지 않게 막는다.
	if (UCameraComponent* MainCamera = PreviewWorld->GetMainCameraComponent())
		MainCamera->SetExternalInputManaged(true);
}

FPreviewScene::~FPreviewScene()
{
	Release();
}

void FPreviewScene::Tick(float DeltaTime)
{
	if (!PreviewWorld)
		return;

	// 디버거 정지나 창 드래그 뒤의 큰 DeltaTime이 파티클을 한 번에 튀게 만들지 않도록 제한한다.
	PreviewWorld->Tick(std::min(DeltaTime, 0.1f));
}

void FPreviewScene::Release()
{
	if (!PreviewWorld)
		return;

	// 전역 일괄 삭제가 이미 월드를 지웠다면 남은 포인터만 버린다. (이중 delete 방지)
	if (GUObjectArray.Find(PreviewWorld) == INDEX_NONE)
	{
		PreviewWorld = nullptr;
		return;
	}

	// UWorld::EndPlay는 호출하지 않는다. 전역 오디오 정지(StopAll)가 에디터·PIE 소리까지 끊기 때문이다.
	// CleanupWorld가 컴포넌트 등록·틱 함수·SceneProxy를 먼저 풀고 액터와 레벨을 삭제한다.
	PreviewWorld->CleanupWorld();
	delete PreviewWorld;
	PreviewWorld = nullptr;
}

bool FPreviewScene::DestroyActor(AActor* Actor)
{
	return PreviewWorld && Actor ? PreviewWorld->DestroyActor(Actor) : false;
}

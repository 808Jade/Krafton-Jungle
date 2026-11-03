#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/Viewports/EditorViewportClient.h"

class UWorld;
class USkeletalMesh;
class ASkeletalMeshActor;

// 스켈레탈 메시 하나를 레벨과 분리된 프리뷰 월드에 띄워 보는 창.
// 렌더는 UEditorEngine::RenderViewports가 이 창의 뷰포트 클라이언트로 한 번 더 그린다.
class FSkeletalMeshViewerPanel : public IEditorPanel
{
public:
	bool Init() override;
	// ImGui 창에서 받은 크기로 렌더 타깃을 맞춘다. 렌더보다 먼저 불려서 이번 프레임 타깃이 바로 쓰인다
	void Tick(float DeltaTime) override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Skeletal Mesh Viewer"; }

	void OpenAsset(USkeletalMesh* Mesh);
	void Shutdown();

	FEditorViewportClient* GetViewportClient() const { return ViewportClient.get(); }
	UWorld* GetPreviewWorld() const { return PreviewWorld; }
	// OnRender에서 ImGui 입력으로 모은 카메라 입력을 꺼내고 누적값(마우스 이동·휠)을 비운다
	FViewportCameraInput ConsumeCameraInput();

private:
	// 메시 바운드가 화면에 들어오게 카메라를 앞쪽에 둔다
	void FocusCamera();
	void GatherCameraInput();

	UWorld* PreviewWorld = nullptr;
	ASkeletalMeshActor* PreviewActor = nullptr;
	TUniquePtr<FEditorViewportClient> ViewportClient;
	uint32 RequestedWidth = 1;
	uint32 RequestedHeight = 1;
	FViewportCameraInput CameraInput;
	bool bHovered = false;
	bool bCameraCaptured = false;
};

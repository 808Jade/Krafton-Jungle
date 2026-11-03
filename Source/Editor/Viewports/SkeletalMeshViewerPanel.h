#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/Viewports/EditorViewportClient.h"

class UWorld;
class USkeletalMesh;
class ASkeletalMeshActor;
class ADirectionalLightActor;
class FLineBatcher;

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

	// 본 트리에서 고른 본. 없으면 INDEX_NONE
	int32 GetSelectedBone() const { return SelectedBone; }
	void SelectBone(int32 BoneIndex) { SelectedBone = BoneIndex; }

	// 언리얼 Persona처럼 관절은 와이어 구, 뼈는 부모→자식 사각뿔로 그린다. 깊이 테스트 없이 그리는 건 호출자 몫
	void DrawBones(FLineBatcher* LineBatcher) const;

private:
	// 메시 바운드가 화면에 들어오게 카메라를 앞쪽에 둔다
	void FocusCamera();
	void GatherCameraInput();

	// 우측 본 계층 트리 (Outliner처럼 클릭으로 선택)
	void DrawBoneTree();
	void DrawBoneNode(const USkeletalMesh* Mesh, int32 BoneIndex);
	// 본 배열은 부모 인덱스만 가지므로 트리를 그리려고 자식 목록을 만든다. 메시가 바뀔 때만 다시 만든다
	void RebuildBoneChildren(const USkeletalMesh* Mesh);
	// 가장 깊고 긴 본 이름이 잘리지 않는 트리 너비
	float CalcBoneTreeWidth();

	UWorld* PreviewWorld = nullptr;
	ASkeletalMeshActor* PreviewActor = nullptr;
	// 카메라를 따라 돌아서 보이는 면을 항상 비춘다
	ADirectionalLightActor* PreviewLight = nullptr;
	TUniquePtr<FEditorViewportClient> ViewportClient;
	uint32 RequestedWidth = 1;
	uint32 RequestedHeight = 1;
	FViewportCameraInput CameraInput;
	bool bHovered = false;
	bool bCameraCaptured = false;

	const USkeletalMesh* BoneTreeMesh = nullptr;
	TArray<TArray<int32>> BoneChildren;
	TArray<int32> RootBones;
	TArray<int32> BoneDepth;
	int32 SelectedBone = INDEX_NONE;
};

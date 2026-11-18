#pragma once
#include "Editor/SkeletalMeshViewer/SkeletalPreviewTabBase.h"
#include <functional>

class USkeletalMesh;

// 스켈레탈 메시 뷰어 탭. (Week11의 FSkeletalMeshPreviewTab에 해당)
// 편집 상태와 프리뷰 월드는 부모(FSkeletalPreviewTabBase)가 가지고, 이 클래스는 하위 패널 6개와 메뉴바 툴바, 기본 배치를 붙인다.
//  - FSkeletalViewportPanel         : 프리뷰 뷰포트
//  - FSkeletonTreePanel             : 본 계층 트리
//  - FSkeletalAssetDetailsPanel     : 메시 정보·머티리얼
//  - FSkeletalPreviewSettingsPanel  : 카메라·조명·표시 설정 (Week11에 없던 패널)
//  - FSkeletalAnimationPanel        : 애니메이션 재생·타임라인·노티파이
//  - FSkeletalBlendSpacePanel       : BlendSpace 미리보기 (Week11에 없던 패널)
// 두 가지로 쓴다.
//  - 메시 하나당 탭 하나. 같은 메시를 다시 열면 기존 탭을 띄운다.
//  - 애니메이션 미리보기 탭(하나). State Machine 에디터의 미리보기가 여기에 나온다. 닫아도 숨기기만 한다.
class FSkeletalMeshViewerTab : public FSkeletalPreviewTabBase
{
public:
	// 메시 탭. InMesh가 nullptr이면 빈 뷰어로 시작한다.
	FSkeletalMeshViewerTab(int32 InSlot, USkeletalMesh* InMesh, std::function<void(USkeletalMesh*)> InOpenMeshRequest);
	// 애니메이션 미리보기 탭. 여러 메시를 오가므로 메시 탭 바를 보여 준다.
	explicit FSkeletalMeshViewerTab(const FString& InId);

	bool Init() override;
	void Tick(float DeltaTime) override;

	// 메시 탭만 에셋으로 중복을 판정한다. 미리보기 탭은 여러 메시를 오가므로 nullptr이다.
	const UObject* GetEditingAsset() const override;

	int32 GetSlot() const { return Slot; }
	bool IsSharedPreviewTab() const { return bSharedPreview; }
	static FString MakeTabId(int32 InSlot);

protected:
	void OnPreRender() override;
	// 메뉴바(Mesh·View Mode·Window·저장 버튼), 미리보기 탭의 메시 탭 바, 빈 뷰어 안내
	void DrawHostContents() override;
	// 왼쪽 Skeleton Tree, 가운데 Viewport, 가운데 아래 Animation·Blend Space, 오른쪽 Asset Details·Preview Scene Settings
	void BuildDefaultLayout(ImGuiID DockSpaceId) override;
	// imgui.ini에 같은 탭 ID의 옛 배치(창 이름이 다르던 이전 버전)가 남아 있으면, 노드는 있지만 지금 패널은 하나도 모른다.
	// 그런 배치도 새로 짠다. 그렇지 않으면 새 패널들이 한 노드에 모두 겹친다.
	bool NeedsLayoutRebuild(const ImGuiDockNode* Node) const override;

private:
	void RefreshTitle();
	void DrawToolbarUI();
	void DrawSaveAsModal();
	// 미리보기 탭 전용: State Machine 미리보기로 여러 메시가 열리므로 메시를 고르는 탭 바를 둔다.
	void DrawAssetTabBar();

	int32 Slot = -1;
	bool bSharedPreview = false;
	USkeletalMesh* InitialMesh = nullptr;
	std::function<void(USkeletalMesh*)> OpenMeshRequest;

	// 패널 창이 아직 만들어지기 전에 기본 배치를 짰는지. (빈 뷰어로 열었거나, 메시를 연 첫 프레임)
	// 그때는 분할 노드에 붙을 창이 없어 ImGui가 빈 노드를 합쳐 버리므로, 패널 창이 생긴 다음 프레임에 한 번 다시 짠다.
	// ini에 저장된 배치를 불러온 경우는 BuildDefaultLayout이 불리지 않으므로 건드리지 않는다.
	bool bLayoutBuiltWithoutWindows = false;

	// Save As New .bin 모달 팝업 상태. 툴바(이 탭)만 쓴다.
	bool bShowSaveAsModal = false;
	char SaveAsNameBuffer[128] = "";

	// 패널 창 이름. BuildDefaultLayout에서 DockBuilder에 넘긴다. (패널 자체는 FEditorTab이 소유한다)
	FString ViewportName, SkeletonTreeName, AssetDetailsName, PreviewSettingsName, AnimationName, BlendSpaceName;
};

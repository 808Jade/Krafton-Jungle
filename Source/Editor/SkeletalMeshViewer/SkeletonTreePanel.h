#pragma once
#include "Editor/EditorUI/EditorPanel.h"
#include "Core/EngineString.h"
#include "Containers/Array.h"

class FSkeletalPreviewTabBase;
struct FSkeletalBone;
class UAnimDataModel;
class UAnimNotify;

// 스켈레탈 메시 에디터 탭의 하위 패널: 본 계층 트리. 검색, 선택, 본 Transform 편집, 바인드 포즈 리셋을 한다. (Week11 SkeletonTreePanel에 해당)
class FSkeletonTreePanel : public IEditorPanel
{
public:
	// InTabId: 소속 탭의 ID. 같은 에디터 탭이 여러 개 열려도 ImGui 창 ID가 겹치지 않게 창 이름 뒤에 붙인다.
	FSkeletonTreePanel(FSkeletalPreviewTabBase& InTab, const FString& InTabId, const ImGuiWindowClass& InWindowClass)
		: Tab(InTab), WindowClass(InWindowClass), PanelName(FString("Skeleton Tree###SkelTree_") + InTabId) {}

	bool Init() override { return true; }
	void Tick(float DeltaTime) override {}
	void OnRender() override;
	const char* GetPanelName() const override { return PanelName.c_str(); }

private:
	void DrawBoneHierarchyUI();
	void DrawBoneNode(int32 BoneIndex, const TArray<FSkeletalBone>& Bones);

	// 참조(집약): 이 패널을 소유한 탭. 편집 상태를 가진다. 탭이 패널보다 오래 산다.
	FSkeletalPreviewTabBase& Tab;
	// 참조(집약): 탭의 WindowClass. 이 패널은 그 탭의 DockSpace에만 도킹된다.
	const ImGuiWindowClass& WindowClass;
	FString PanelName;
	// 본 이름 검색어. 이 패널만 쓴다.
	char BoneSearchFilter[64] = "";
};

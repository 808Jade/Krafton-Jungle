#pragma once
#include "Editor/EditorUI/EditorPanel.h"
#include "Core/EngineString.h"
#include "Containers/Array.h"

class FSkeletalPreviewTabBase;
struct FSkeletalBone;
class UAnimDataModel;
class UAnimNotify;

// 스켈레탈 메시 에디터 탭의 하위 패널: 메시 통계(본·정점·삼각형·섹션 수)와 머티리얼 슬롯·샘플러 설정. (Week11 PersonaDetailsPanel에 해당)
class FSkeletalAssetDetailsPanel : public IEditorPanel
{
public:
	// InTabId: 소속 탭의 ID. 같은 에디터 탭이 여러 개 열려도 ImGui 창 ID가 겹치지 않게 창 이름 뒤에 붙인다.
	FSkeletalAssetDetailsPanel(FSkeletalPreviewTabBase& InTab, const FString& InTabId, const ImGuiWindowClass& InWindowClass)
		: Tab(InTab), WindowClass(InWindowClass), PanelName(FString("Asset Details###SkelDetails_") + InTabId) {}

	bool Init() override { return true; }
	void Tick(float DeltaTime) override {}
	void OnRender() override;
	const char* GetPanelName() const override { return PanelName.c_str(); }

private:
	void DrawMeshDetailsUI();

	// 참조(집약): 이 패널을 소유한 탭. 편집 상태를 가진다. 탭이 패널보다 오래 산다.
	FSkeletalPreviewTabBase& Tab;
	// 참조(집약): 탭의 WindowClass. 이 패널은 그 탭의 DockSpace에만 도킹된다.
	const ImGuiWindowClass& WindowClass;
	FString PanelName;
};

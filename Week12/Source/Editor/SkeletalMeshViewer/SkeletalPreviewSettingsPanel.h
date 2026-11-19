#pragma once
#include "Editor/EditorUI/EditorPanel.h"
#include "Core/EngineString.h"
#include "Containers/Array.h"

class FSkeletalPreviewTabBase;
struct FSkeletalBone;
class UAnimDataModel;
class UAnimNotify;

// 스켈레탈 메시 에디터 탭의 하위 패널: 프리뷰 카메라·기즈모·본 표시·조명(방향광/하늘광)·노출 설정. Week11에는 없던 패널이다.
class FSkeletalPreviewSettingsPanel : public IEditorPanel
{
public:
	// InTabId: 소속 탭의 ID. 같은 에디터 탭이 여러 개 열려도 ImGui 창 ID가 겹치지 않게 창 이름 뒤에 붙인다.
	FSkeletalPreviewSettingsPanel(FSkeletalPreviewTabBase& InTab, const FString& InTabId, const ImGuiWindowClass& InWindowClass)
		: Tab(InTab), WindowClass(InWindowClass), PanelName(FString("Preview Scene Settings###SkelSettings_") + InTabId) {}

	bool Init() override { return true; }
	void Tick(float DeltaTime) override {}
	void OnRender() override;
	const char* GetPanelName() const override { return PanelName.c_str(); }

private:
	void DrawLightingSettingsUI();

	// 참조(집약): 이 패널을 소유한 탭. 편집 상태를 가진다. 탭이 패널보다 오래 산다.
	FSkeletalPreviewTabBase& Tab;
	// 참조(집약): 탭의 WindowClass. 이 패널은 그 탭의 DockSpace에만 도킹된다.
	const ImGuiWindowClass& WindowClass;
	FString PanelName;
};

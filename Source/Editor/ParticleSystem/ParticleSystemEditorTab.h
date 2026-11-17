#pragma once
#include "Editor/EditorUI/EditorTab.h"
#include "Editor/ParticleSystem/ParticleEditorContext.h"
#include <functional>

class FParticleSystemViewportPanel;
class FParticleSystemEmitterPanel;
class FParticleSystemDetailsPanel;
class FParticleCurveEditorPanel;
class UParticleSystem;
class UTexture2D;

// 파티클 시스템 에디터 탭 하나. 에셋 하나를 편집한다. (Cascade 창 하나에 해당)
// 호스트 창에 ① 메뉴바 ② 툴바를 그리고, 탭의 DockSpace에 ③ 뷰포트 ④ 이미터 ⑤ 디테일 하위 패널을 둔다.
// 프리뷰 월드와 선택 상태는 탭마다 따로 가진다. 그래서 탭 여러 개가 서로 다른 에셋을 동시에 편집할 수 있다.
class FParticleSystemEditorTab : public FEditorTab
{
public:
	// File > Open에서 고른 에셋을 엔진에 넘긴다. 엔진이 이미 열린 탭을 띄우거나 새 탭을 연다.
	using FOpenAssetRequest = std::function<void(UParticleSystem*)>;

	// InSlot: 탭 ID 번호. 닫힌 번호를 다시 쓰므로 imgui.ini의 하위 패널 배치를 재사용한다.
	FParticleSystemEditorTab(int32 InSlot, UParticleSystem* InAsset, FOpenAssetRequest InOpenAssetRequest);
	~FParticleSystemEditorTab() override;

	bool Init() override;
	void Tick(float DeltaTime) override;

	const UObject* GetEditingAsset() const override;
	bool IsHoveredOrInteracting() const override;
	void CollectViews(TArray<FEditorTabView>& OutViews) override;
	// 타깃은 Device 종료 전에, 프리뷰 월드는 전역 UObject 일괄 삭제 전에 놓는다. 여러 번 불러도 안전하다.
	void ReleaseResources() override;

	int32 GetSlot() const { return Slot; }
	static FString MakeTabId(int32 InSlot);

protected:
	void DrawHostContents() override;
	ImGuiWindowFlags GetHostWindowFlags() const override;
	// Cascade 배치: 왼쪽 위 뷰포트, 왼쪽 아래 디테일, 오른쪽 위 이미터, 오른쪽 아래 커브 에디터
	void BuildDefaultLayout(ImGuiID DockSpaceId) override;
	bool NeedsLayoutRebuild(const ImGuiDockNode* Node) const override;

private:
	void DrawMenuBar();
	void DrawToolBar();
	// 툴바 그룹: Cascade 툴바 순서(에셋·시뮬 / 표시 / LOD)를 따른다. Undo·Redo, Thumbnail, Origin Axis는 두지 않는다.
	void DrawAssetGroup();
	void DrawViewGroup();
	void DrawLODGroup();
	void DrawSimulationGroup();
	void LoadToolbarIcons();
	// 툴바 Save 버튼과 Ctrl+S가 부른다. 파일이 있는 에셋은 바로 덮어쓰고, 없으면 Save As 창을 연다.
	void RequestSave();
	void RequestSaveAs();
	// 이름을 입력받는 Save As 모달. 호스트 창 안에서 매 프레임 호출한다.
	void DrawSaveAsPopup();
	// 탭 제목을 편집 중인 에셋 이름으로 맞춘다. Save As 뒤에 이름이 바뀐다.
	void RefreshTitle();

	int32 Slot = 0;
	// 참조(집약): Init에서 프리뷰에 씌울 에셋. 소유자는 UAssetManager다.
	UParticleSystem* InitialAsset = nullptr;
	FOpenAssetRequest OpenAssetRequest;

	// 소유(합성): 하위 패널들이 공유하는 편집 상태와 프리뷰 월드. 탭과 수명이 같다.
	FParticleEditorContext Context;

	// 참조(집약): Panels(FEditorTab 소유) 안의 하위 패널. 배치·렌더 연결·툴바에 쓴다.
	FParticleSystemViewportPanel* ViewportPanel = nullptr;
	FParticleSystemEmitterPanel* EmitterPanel = nullptr;
	FParticleSystemDetailsPanel* DetailsPanel = nullptr;
	FParticleCurveEditorPanel* CurveEditorPanel = nullptr;

	// 참조(집약): 툴바 아이콘. 텍스처는 UAssetManager가 소유하고 캐시한다.
	struct FToolbarIcons
	{
		UTexture2D* Save = nullptr;
		UTexture2D* RestartSim = nullptr;
		UTexture2D* RestartLevel = nullptr;
		UTexture2D* BackgroundColor = nullptr;
		UTexture2D* RegenLOD = nullptr;
		UTexture2D* RegenLODPercent = nullptr;
		UTexture2D* LowestLOD = nullptr;
		UTexture2D* LowerLOD = nullptr;
		UTexture2D* AddLODBefore = nullptr;
		UTexture2D* AddLODAfter = nullptr;
		UTexture2D* HigherLOD = nullptr;
		UTexture2D* HighestLOD = nullptr;
		UTexture2D* DeleteLOD = nullptr;
		UTexture2D* Pause = nullptr;
		UTexture2D* Play = nullptr;
	};
	FToolbarIcons Icons;

	// Save As 모달 상태
	bool bOpenSaveAsPopup = false;
	char SaveAsName[128] = {};
	FString SaveAsError;
};

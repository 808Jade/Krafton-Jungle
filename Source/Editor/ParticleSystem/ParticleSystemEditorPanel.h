#pragma once
#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/ParticleSystem/ParticleEditorContext.h"

class FParticleSystemViewportPanel;
class UTexture2D;

// 파티클 시스템 에디터의 호스트 창. ① 메뉴바 ② 툴바를 직접 그리고, 내부 DockSpace에
// ③ 뷰포트 ④ 이미터 ⑤ 디테일 하위 패널을 소유해 루프로 그린다.
// FEditorUI 입장에서는 IEditorPanel 하나이며, 하위 패널은 FEditorUI의 이름 분류를 거치지 않는다.
class FParticleSystemEditorPanel : public IEditorPanel
{
public:
	FParticleSystemEditorPanel();
	~FParticleSystemEditorPanel() override;
	FParticleSystemEditorPanel(const FParticleSystemEditorPanel&) = delete;
	FParticleSystemEditorPanel& operator=(const FParticleSystemEditorPanel&) = delete;

	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Particle System Editor"; }

	// 콘텐츠 드로어에서 .particle을 더블클릭하면 부른다. 에디터 창을 열고 앞으로 가져온 뒤 그 에셋을 프리뷰한다.
	// 이미 불러온 에셋이면 메모리의 것을 그대로 쓴다. (저장하지 않은 수정도 유지된다)
	void OpenAssetFile(const FString& Path);

	// 프리뷰 월드와 GPU 타깃을 해제한다. 전역 UObject 일괄 삭제와 Device 종료 전에 엔진이 호출한다.
	void ReleaseResources();

	// 프리뷰 뷰포트가 마우스 입력을 쓰는 동안 레벨 뷰포트의 카메라·피킹 입력을 막는 데 쓴다.
	bool IsHoveredOrInteracting() const;

	// --- 엔진이 프리뷰를 그릴 때 쓰는 입력 ---
	UWorld* GetPreviewWorld() const { return Context.GetPreviewWorld(); }
	// 호스트가 보이지 않으면 nullptr이다. 그릴 대상이 없다는 뜻이다.
	FParticleSystemViewportPanel* GetViewportPanel() const;

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
	// 내부 DockSpace의 초기 배치. 저장된 배치가 없거나 Reset Layout을 고르면 실행한다.
	void BuildDefaultLayout(unsigned int DockSpaceId);

	// 소유(합성): 하위 패널들이 공유하는 편집 상태와 프리뷰 월드
	FParticleEditorContext Context;

	// 소유(합성): ③④⑤ 하위 패널. 이 에디터 안에서만 쓰이고 함께 파괴된다.
	TArray<TUniquePtr<IEditorPanel>> SubPanels;
	// 참조(집약): SubPanels 안의 뷰포트. 엔진 렌더 연결과 툴바의 카메라 리셋에 쓴다.
	FParticleSystemViewportPanel* ViewportPanel = nullptr;

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

	bool bHostVisible = false;
	bool bResetLayout = false;
	// OpenAssetFile 다음 프레임에 호스트 창을 앞으로 가져온다. (다른 탭 뒤에 도킹돼 있어도 보이게)
	bool bFocusNextFrame = false;

	// Save As 모달 상태
	bool bOpenSaveAsPopup = false;
	char SaveAsName[128] = {};
	FString SaveAsError;

	bool bPendingRegenDuplicate = true;
};

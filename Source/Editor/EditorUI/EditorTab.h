#pragma once
#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/HitoriEd/EditorSceneView.h"

class UObject;
class UWorld;
struct FRenderingInfo;

// 엔진이 탭 대신 그려 줄 프리뷰 화면 하나. 포인터는 모두 비소유이고 이번 프레임 렌더 동안만 유효하다.
// View.ShadowViewId는 엔진이 채운다.
struct FEditorTabView
{
	UWorld* World = nullptr;
	FEditorSceneView View;
	const FRenderingInfo* RenderingInfo = nullptr;
};

struct FEditorTabSettings
{
	// 탭에 보이는 이름. 바뀌어도 된다. (예: Save As 뒤 에셋 이름)
	FString Title;
	// imgui.ini에 창 배치가 저장되는 키. 탭이 살아 있는 동안 바뀌면 안 된다.
	FString Id;
	bool bClosable = true;

	// true면 창 이름을 Title 그대로 쓴다. (기존 "Level Editor" 창 배치를 그대로 쓰기 위해)
	bool bTitleIsWindowName = false;
	// 탭 안 DockSpace의 이름. 비우면 "TabDockSpace"다. 호스트 창 ID 아래에서 만들어지므로 탭끼리 겹치지 않는다.
	FString DockSpaceName;
	// 하위 패널을 묶는 WindowClass의 이름. 비우면 Id를 쓴다.
	FString WindowClassName;
	// true면 처음 그릴 때 레벨 에디터 옆 탭 묶음에 강제로 붙인다. (새로 여는 에셋 에디터 탭)
	bool bDockOnFirstRender = true;
	// true면 탭이 가려져도 하위 패널의 OnRender를 부른다. 패널이 자기 창 상태(Hover 등)를 직접 갱신해야 할 때 쓴다.
	bool bRenderPanelsWhenHidden = false;
	// true면 호스트 창의 WindowPadding을 0으로 둔다.
	bool bZeroHostPadding = false;
};

// 에셋 에디터 하나의 탭. 호스트 창 하나와 그 안의 DockSpace, 하위 패널들을 소유한다.
// 호스트 창은 레벨 에디터·기존 에셋 에디터 창과 같은 루트 DockSpace에 탭으로 붙는다.
// 하위 패널은 탭마다 다른 WindowClass를 받아 자기 탭의 DockSpace에만 도킹된다.
// (Week11의 FEditorTab과 같은 역할. 탭 줄은 별도 TabBar 대신 기존 루트 DockSpace의 도킹 탭을 쓴다.)
class FEditorTab
{
public:
	explicit FEditorTab(FEditorTabSettings InSettings);
	virtual ~FEditorTab() = default;
	FEditorTab(const FEditorTab&) = delete;
	FEditorTab& operator=(const FEditorTab&) = delete;

	virtual bool Init() { return true; }
	virtual void Tick(float DeltaTime);
	// 호스트 창 → 내부 DockSpace → 하위 패널 순서로 그린다. DefaultDockId는 처음 뜰 때 붙을 노드다.
	void Render(ImGuiID DefaultDockId);

	// 패널을 만들어 Init한 뒤 raw pointer를 돌려준다. (소유권 = 탭)
	template <typename T, typename... TArgs>
	T* AddPanel(TArgs&&... Args)
	{
		TUniquePtr<T> NewPanel = MakeUnique<T>(std::forward<TArgs>(Args)...);
		T* Ret = NewPanel.get();
		Ret->Init();
		Panels.Add(std::move(NewPanel));
		return Ret;
	}
	const TArray<TUniquePtr<IEditorPanel>>& GetPanels() const { return Panels; }

	const FString& GetId() const { return Settings.Id; }
	const FString& GetTitle() const { return Settings.Title; }
	void SetTitle(FString InTitle) { Settings.Title = std::move(InTitle); }
	// 보이는 이름은 바뀌어도 ImGui 창 ID와 저장된 배치는 유지된다. ("Title###Id")
	FString GetWindowName() const { return Settings.bTitleIsWindowName ? Settings.Title : Settings.Title + "###" + Settings.Id; }
	bool CanClose() const { return Settings.bClosable; }

	bool IsOpen() const { return bOpen; }
	// 닫혀 있거나 다른 탭에 가려졌으면 false다.
	bool IsVisible() const { return bOpen && bHostVisible; }
	void RequestFocus() { bFocusRequested = true; }
	// 다음 Render에서 BuildDefaultLayout으로 배치를 다시 만든다.
	void RequestLayoutReset() { bResetLayout = true; }

	// 같은 에셋을 두 번 열면 새 탭 대신 기존 탭을 띄우는 데 쓴다.
	virtual const UObject* GetEditingAsset() const { return nullptr; }
	// 탭의 프리뷰 뷰포트가 마우스를 쓰는 중이면 true. 엔진이 레벨 뷰포트 입력을 막는 데 쓴다.
	virtual bool IsHoveredOrInteracting() const { return false; }
	// 엔진이 이번 프레임에 그려 줄 프리뷰 화면을 모은다.
	virtual void CollectViews(TArray<FEditorTabView>& OutViews) {}
	// GPU 타깃·프리뷰 월드를 놓는다. 탭을 지우기 전과 엔진 종료(UObject 일괄 삭제·Device 종료) 전에 불린다.
	virtual void ReleaseResources() {}

protected:
	// 호스트 창 안(메뉴바·툴바 자리)을 그린다. 창이 보일 때만 불린다.
	virtual void DrawHostContents() {}
	virtual ImGuiWindowFlags GetHostWindowFlags() const { return ImGuiWindowFlags_MenuBar; }
	// 저장된 배치가 없거나 망가졌으면 true. 기본은 노드가 없거나 비어 있을 때다.
	virtual bool NeedsLayoutRebuild(const ImGuiDockNode* Node) const;
	// imgui.ini에 이 탭의 배치가 없거나 리셋을 요청했을 때 부른다. 기본은 모든 패널을 한 노드에 겹친다.
	virtual void BuildDefaultLayout(ImGuiID DockSpaceId);
	// 하위 패널에 줄 WindowClass. 기본은 탭 공통 클래스다. (레벨 탭의 Viewports처럼 패널마다 다르게 줄 때 덮어쓴다)
	virtual const ImGuiWindowClass* GetPanelWindowClass(const IEditorPanel& Panel) const { return &WindowClass; }

	const ImGuiWindowClass& GetWindowClass() const { return WindowClass; }

	// 소유(합성): 이 탭의 하위 패널. 탭이 사라지면 함께 사라진다.
	TArray<TUniquePtr<IEditorPanel>> Panels;

private:
	FEditorTabSettings Settings;
	ImGuiWindowClass WindowClass;
	bool bOpen = true;
	bool bHostVisible = false;
	bool bFocusRequested = false;
	bool bResetLayout = false;
	bool bDockOnFirstRender = true;
};

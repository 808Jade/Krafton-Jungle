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

	const FString& GetId() const { return Settings.Id; }
	const FString& GetTitle() const { return Settings.Title; }
	void SetTitle(FString InTitle) { Settings.Title = std::move(InTitle); }
	// "Title###Id": 보이는 이름은 바뀌어도 ImGui 창 ID와 저장된 배치는 유지된다.
	FString GetWindowName() const { return Settings.Title + "###" + Settings.Id; }

	bool IsOpen() const { return bOpen; }
	// 닫혀 있거나 다른 탭에 가려졌으면 false다.
	bool IsVisible() const { return bOpen && bHostVisible; }
	void RequestFocus() { bFocusRequested = true; }

	// 같은 에셋을 두 번 열면 새 탭 대신 기존 탭을 띄우는 데 쓴다.
	virtual const UObject* GetEditingAsset() const { return nullptr; }
	// 탭의 프리뷰 뷰포트가 마우스를 쓰는 중이면 true. 엔진이 레벨 뷰포트 입력을 막는 데 쓴다.
	virtual bool IsHoveredOrInteracting() const { return false; }
	// 엔진이 이번 프레임에 그려 줄 프리뷰 화면을 모은다.
	virtual void CollectViews(TArray<FEditorTabView>& OutViews) {}
	// GPU 타깃·프리뷰 월드를 놓는다. 탭을 지우기 전과 엔진 종료(UObject 일괄 삭제·Device 종료) 전에 불린다.
	virtual void ReleaseResources() {}

protected:
	template <typename T, typename... TArgs>
	T* AddPanel(TArgs&&... Args)
	{
		TUniquePtr<T> NewPanel = MakeUnique<T>(std::forward<TArgs>(Args)...);
		T* Ret = NewPanel.get();
		Panels.Add(std::move(NewPanel));
		return Ret;
	}

	// 호스트 창 안(메뉴바·툴바 자리)을 그린다. 창이 보일 때만 불린다.
	virtual void DrawHostContents() {}
	virtual ImGuiWindowFlags GetHostWindowFlags() const { return ImGuiWindowFlags_MenuBar; }
	// imgui.ini에 이 탭의 배치가 없거나 리셋을 요청했을 때 부른다. 기본은 모든 패널을 한 노드에 겹친다.
	virtual void BuildDefaultLayout(ImGuiID DockSpaceId);
	void RequestLayoutReset() { bResetLayout = true; }

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

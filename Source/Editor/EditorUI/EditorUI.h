#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/EditorUI/EditorTab.h"
#include "Editor/LevelEditor/LevelEditorToolbar.h"
#include <functional>

class FEditorUI
{
public:
	bool Init(bool bInUseDockSpace = true, bool bInPassthruCentralNode = false);
	void Tick(float DeltaTime);
	void OnRender();

	template <typename T>
	T* AddEditorPanel()
	{
		TUniquePtr<T> newPanel = MakeUnique<T>();
		T* Ret = newPanel.get();
		Ret->Init();
		Panels.Add(std::move(newPanel));
		
		return Ret;
	}

	// 에셋 에디터 탭을 추가하고 앞으로 가져온다. Init이 실패하면 추가하지 않고 nullptr을 돌려준다.
	template <typename T, typename... TArgs>
	T* AddTab(TArgs&&... Args)
	{
		TUniquePtr<T> NewTab = MakeUnique<T>(std::forward<TArgs>(Args)...);
		if (!NewTab->Init())
		{
			NewTab->ReleaseResources();
			return nullptr;
		}
		T* Ret = NewTab.get();
		Ret->RequestFocus();
		Tabs.Add(std::move(NewTab));
		return Ret;
	}
	const TArray<TUniquePtr<FEditorTab>>& GetTabs() const { return Tabs; }
	// 탭 0. Init에서 만들고 엔진이 끝날 때까지 지우지 않는다. 레벨 패널은 여기에 추가한다.
	class FLevelEditorTab* GetLevelTab() const;
	bool IsAnyTabHoveredOrInteracting() const;
	void CollectTabViews(TArray<FEditorTabView>& OutViews) const;
	void RenderTabCustomPreviews() const;
	// 엔진 종료 시 UObject 일괄 삭제와 Device 종료 전에 부른다.
	void ReleaseTabResources();

	// Window 메뉴 맨 아래에 붙는 항목. 에셋 에디터 탭을 새로 여는 명령 등을 엔진이 등록한다.
	void AddWindowMenuCommand(FString Label, std::function<void()> Command)
	{
		WindowMenuCommands.Add({ std::move(Label), std::move(Command) });
	}

	FLevelEditorToolbar* GetToolbar() const { return Toolbar.get(); }

	void SetNewSceneCallback(std::function<void()> InCallback) { OnNewScene = InCallback; }
	void SetOpenSceneCallback(std::function<void()> InCallback) { OnOpenScene = InCallback; }
	void SetSaveSceneCallback(std::function<void()> InCallback) { OnSaveScene = InCallback; }
	void SetSaveSceneAsCallback(std::function<void()> InCallback) { OnSaveSceneAs = InCallback; }

private:
	void DrawMainMenuBar();
	// 레벨 탭을 뺀 탭을 그리고, 닫힌 탭의 자원을 놓은 뒤 지운다.
	void RenderTabs(ImGuiID RootWorkspaceDockId);

private:
	bool bUseDockSpace = true;
	bool bAnimGraphInitialDockPending = true;
	bool bPassthruCentralNode = false;

	// 소유(합성): 아직 탭으로 옮기지 않은 에셋 에디터 패널 (Skeletal Mesh Viewer 등)
	TArray<TUniquePtr<IEditorPanel>> Panels;
	// 소유(합성): 탭. 0번은 레벨 탭이고, 나머지는 에셋 에디터 탭이다. 에셋 에디터 탭을 닫으면 여기서 지운다.
	TArray<TUniquePtr<FEditorTab>> Tabs;
	struct FWindowMenuCommand
	{
		FString Label;
		std::function<void()> Command;
	};
	TArray<FWindowMenuCommand> WindowMenuCommands;
	TUniquePtr<FLevelEditorToolbar> Toolbar;

	std::function<void()> OnNewScene;
	std::function<void()> OnOpenScene;
	std::function<void()> OnSaveScene;
	std::function<void()> OnSaveSceneAs;
};

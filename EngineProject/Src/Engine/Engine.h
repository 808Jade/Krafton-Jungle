#pragma once

#include "Core/Window.h"
#include "String.h"
#include "Core/Types.h"

#include "World.h"
#include "Render/Renderer.h"
#include "Render/RenderDevice.h"
#include "Render/Swapchain.h"
#include "ImGuiRenderer.h"
#include "Editor/GridRenderer.h"
#include "Editor/GizmoRenderer.h"
#include "Render/LineBatcher.h"

#include "Editor/EditorUI.h"
#include "Editor/ConsolePanel.h"
#include "Editor/PropertyPanel.h"
#include "Editor/ControlPanel.h"
#include "Editor/ConfigPanel.h"
#include "Editor/ViewportPanel.h"
#include "Editor/AssetBrowserPanel.h"

#include "Editor/Outline.h"
#include "Editor/OutLineRenderer.h"

//Temp
#include "Text/FontAtlas.h"
#include "Text/TextRenderer.h"

#include "../Editor/SceneOutliner.h"

struct FWindowContext
{
	TUniquePtr<FWindow> Window;
	TUniquePtr<FSwapchain> Swapchain;
};

class Engine
{
public:
	bool Init(HINSTANCE hInstance);
	void Run();
	void UpdateGizmoAndPicking(FMatrix& ViewProj);
	void RenderFrame(FMatrix& ViewProj, TQueue<FRenderPacket>& RenderQueue);
	void Shutdown();

	static FConsolePanel* GetConsolePanel() { return ConsolePanel; }

	//윈도우 크기 변경 처리
	void HandleMainWindow();

private:
	bool bIsRunning = false;
	bool bIsResized = false;

	// 가장 먼저 선언 → 가장 나중에 해제 (Swapchain, 버퍼들보다 오래 살아야 함)
	TUniquePtr<FRenderDevice> RenderDevice;

	TArray<FWindowContext> Windows;
	FWindow* MainWindow;
	FSwapchain* MainWindowSC;

	UWorld* World;

	

	TUniquePtr<FEditorUI> EditorUI;

	TUniquePtr<FRenderer> Renderer;
	TUniquePtr<FImGuiRenderer> ImGuiRenderer;
	TUniquePtr<FGridRenderer> GridRenderer;
	TUniquePtr<FGizmoRenderer> GizmoRenderer;
	TUniquePtr<FTextRenderer> TextRenderer;
	TUniquePtr<FLineBatcher> LineBatcher;
	//TUniquePtr<FGizmo> Gizmo;
	TUniquePtr<FGizmo> Gizmo;
	TUniquePtr<FOutline> Outline;
	TUniquePtr<FOutlineRenderer> OutlineRenderer;

	TUniquePtr<FShader> TextShader;
	UFontAtlas* Atlas;



	// UE_LOG 매크로용 전역 콘솔
	inline static FConsolePanel* ConsolePanel = nullptr;
	FPropertyPanel* PropertyPanel = nullptr;
	FControlPanel* ControlPanel = nullptr;
	FConfigPanel* ConfigPanel = nullptr;
	FViewportPanel* ViewportPanel= nullptr;
	FSceneOutliner* SceneOutliner = nullptr;
	FSceneManager* SceneManager = nullptr;
	FAssetBrowserPanel* AssetBrowser = nullptr;
};


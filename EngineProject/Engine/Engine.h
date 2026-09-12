#pragma once

#include "Core/Window.h"
#include "String.h"
#include "Core/Types.h"

#include "World.h"
#include "Render/Renderer.h"
#include "ImGuiRenderer.h"
#include "Editor/GridRenderer.h"
#include "Editor/GizmoRenderer.h"
#include "Render/DebugRenderer.h"

#include "Editor/EditorUI.h"
#include "Editor/ConsolePanel.h"
#include "Editor/PropertyPanel.h"
#include "Editor/ControlPanel.h"

#include "Editor/Outline.h"
#include "Editor/OutLineRenderer.h"

//Temp
#include "Text/FontAtlas.h"
#include "Text/TextRenderer.h"

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
	void HandleWindowResize();

private:
	bool bIsRunning = false;
	bool bIsResized = false;
	
	TUniquePtr<Window> MainWindow;

	UWorld* World;

	TUniquePtr<FEditorUI> EditorUI;

	TUniquePtr<FRenderer> Renderer;
	TUniquePtr<FImGuiRenderer> ImGuiRenderer;
	TUniquePtr<FGridRenderer> GridRenderer;
	TUniquePtr<FGizmoRenderer> GizmoRenderer;
	TUniquePtr<FTextRenderer> TextRenderer;
	TUniquePtr<FDebugRenderer> DebugRenderer;
	//TUniquePtr<FGizmo> Gizmo;
	TUniquePtr<FGizmo> Gizmo;
	TUniquePtr<FOutline> Outline;
	TUniquePtr<FOutlineRenderer> OutlineRenderer;

	TSharedPtr<FShader> TextShader;
	TSharedPtr<FFontAtlas> Atlas;

	// UE_LOG 매크로용 전역 콘솔
	inline static FConsolePanel* ConsolePanel = nullptr;
	FPropertyPanel* PropertyPanel = nullptr;
	FControlPanel* ControlPanel = nullptr;
};


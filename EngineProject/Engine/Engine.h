#pragma once

#include "Core/Window.h"
#include "String.h"
#include "Core/Types.h"
#include "Core/Containers.h"

#include "World.h"
#include "Render/Renderer.h"
#include "ImGuiRenderer.h"
#include "Editor/GridRenderer.h"
#include "Editor/GizmoRenderer.h"

#include "Editor/EditorUI.h"
#include "Editor/ConsolePanel.h"
#include "Editor/PropertyPanel.h"
#include "Editor/ControlPanel.h"

#include "Editor/Outline.h"
#include "Editor/OutLineRenderer.h"

class Engine
{
public:
	bool Init(HINSTANCE hInstance);
	void Run();
	void Shutdown();

	static FConsolePanel* GetConsolePanel() { return ConsolePanel; }

	void OnWindowResized(uint32 Width, uint32 Height);

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
	//TUniquePtr<FGizmo> Gizmo;
	TSharedPtr<FGizmo> Gizmo;
	TUniquePtr<FOutline> Outline;
	TUniquePtr<FOutlineRenderer> OutlineRenderer;

	inline static FConsolePanel* ConsolePanel = nullptr;
	inline static FPropertyPanel* PropertyPanel = nullptr;
	inline static FControlPanel* ControlPanel = nullptr;

	TSharedPtr<FVertexBuffer> vb;
	TSharedPtr<FIndexBuffer> ib;
	TSharedPtr<FMesh> Mesh;
	TSharedPtr<FShader> Shader;
};


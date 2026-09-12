#include "EnginePCH.h"

#include "Engine.h"
#include "Core/EngineStatics.h"
#include "Core/EngineTimer.h"
#include "Input/InputSystem.h"

#include "ObjectSystem/ObjectFactory.h"

#include "Render/GeometryGenerator.h"

#include "World.h"

#include "Render/Renderer.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

#include "Asset/AssetManager.h"

bool Engine::Init(HINSTANCE hInstance)
{
	EditorUI = MakeUnique<FEditorUI>();
	EditorUI->Init();

	ConsolePanel = EditorUI->AddEditorPanel<FConsolePanel>();
	ConsolePanel->AddLog(ELogVerbosity::Info, "Engine Initialize...");

	// Create Main Window
	LOG(Info, "Create Main Window...");
	MainWindow = MakeUnique<Window>();
	if (!MainWindow->Create(hInstance, 1280, 720, L"Engine"))
	{
		LOG(Error, "Failed To Create Main Window!");
		return false;
	}
	LOG(Info, "Success!");

	LOG(Info, "Initialize Renderer...");
	Renderer = MakeUnique<FRenderer>();
	if (!Renderer->Init(MainWindow->GetHandle()))
	{
		LOG(Error, "Failed To Initialize Renderer!");

	}
	LOG(Info, "Success!");


	LOG(Info, "Initialize AssetManager...");
	UAssetManager::Get().Init(Renderer.get());
	LOG(Info, "Success!");


	LOG(Info, "Initialize ImGui...");
	ImGuiRenderer = MakeUnique<FImGuiRenderer>();
	if (!ImGuiRenderer->Init(MainWindow->GetHandle(), Renderer->GetDevice(), Renderer->GetDeviceContext()))
	{
		LOG(Error, "Failed To Initialize ImGui!");

	}
	LOG(Info, "Success!");

	GridRenderer = MakeUnique<FGridRenderer>();
	GridRenderer->Init(Renderer.get());

	GizmoRenderer = MakeUnique<FGizmoRenderer>();
	GizmoRenderer->Init(Renderer.get());

	Gizmo = MakeUnique<FGizmo>();

	// 필요한 Panel들 추가후 raw pointer 반환(소유권 = EditorUI)
	PropertyPanel = EditorUI->AddEditorPanel<FPropertyPanel>();
	ControlPanel = EditorUI->AddEditorPanel<FControlPanel>();

	// OutLine
	OutlineRenderer = MakeUnique<FOutlineRenderer>();
	OutlineRenderer->Init(Renderer.get());

	ConfigPanel = EditorUI->AddEditorPanel<FConfigPanel>();

	Outline = MakeUnique<FOutline>();

	TextShader = Renderer->CreateShader(L"Shader/TextShader.hlsl", FTextVertex::GetLayout());
	Atlas = Renderer->LoadFontAtlas("atlas.json", "atlas.png");

	TextRenderer = MakeUnique<FTextRenderer>();
	TextRenderer->Init(Renderer.get());
	
	// Scene
	World = FObjectFactory::ConstructObject<UWorld>();
	World->Init();

	DebugRenderer = MakeUnique<FDebugRenderer>();
	DebugRenderer->Init(Renderer.get(), World);

	PropertyPanel->SetWorld(World);
	ControlPanel->FControlPanel::World = World;
	ControlPanel->SetGizmo(Gizmo.get());

	ConfigPanel->SetWorld(World);

	ControlPanel->SetSceneClearCallback([&]() {
		Gizmo->SetTarget(nullptr);
		Outline->SetTarget(nullptr);
		PropertyPanel->SetTarget(nullptr);
		}
	);

	bIsRunning = true;

	return true;
}

void Engine::Run()
{
	EngineTimer::Init();

	LOG(Info, "{}", "Hello, World!");
	LOG(Info, "{}", FName().ToString());

	FMatrix Mat;
	Mat.SetIdentity();

	UPrimitiveComponent* OutlineComponent = nullptr;


	while (bIsRunning)
	{
		EngineTimer::Tick();
		float DeltaTime = EngineTimer::GetDeltaTime();
		ControlPanel->FControlPanel::DeltaTime = DeltaTime;

		FInputSystem::UpdateInputStates();

		//Check Swapchain Resize
		MainWindow->ProcessMessage(bIsRunning);

		//메시지 처리 직후 리사이즈 검사
		HandleWindowResize();

		//Update World
		World->Tick(DeltaTime);
		//Update Editor
		EditorUI->Tick(DeltaTime);
		GridRenderer->SetGridSpace(ConfigPanel->GetSettings().GridSpacing);

		FMatrix VP = World->GetMainCamera()->GetCameraComponent()->GetViewProjectionMatrix();

		UpdateGizmoAndPicking(VP);

		TQueue<FRenderPacket> RenderQueue;
		World->GatherRenderPackets(RenderQueue);

		//BeginRendering
		RenderFrame(VP, RenderQueue);
	}
}

void Engine::UpdateGizmoAndPicking(FMatrix& ViewProj)
{
	FRay ray = World->GetMainCamera()->GetCameraComponent()->DeProjection(FInputSystem::GetMouseX(), FInputSystem::GetMouseY(), MainWindow->GetWidth(), MainWindow->GetHeight());
	FVector2 mousePos(FInputSystem::GetMouseX(), FInputSystem::GetMouseY());
	bool bMouseDown = FInputSystem::IsMouseDown(EMouseButton::Left);

	Gizmo->Update(ray, mousePos, ViewProj, MainWindow->GetWidth(), MainWindow->GetHeight(), bMouseDown, World->GetMainCamera()->GetCameraComponent());

	if (FInputSystem::IsMousePressed(EMouseButton::Left) && !Gizmo->IsUsing() && Gizmo->GetHoveredAxis() < 0 && !ImGui::GetIO().WantCaptureMouse)
	{
		UPrimitiveComponent* PickedComponent = World->TryPicking(MainWindow->GetWidth(), MainWindow->GetHeight());
		Gizmo->SetTarget(PickedComponent);
		Outline->SetTarget(PickedComponent);
		PropertyPanel->SetTarget(PickedComponent);
	}
}

void Engine::RenderFrame(FMatrix& ViewProj, TQueue<FRenderPacket>& RenderQueue)
{
	Renderer->BeginFrame();

	if (ConfigPanel->GetSettings().bDrawBoundingBox)
	{
		DebugRenderer->OnRender(ViewProj);
	}
	GridRenderer->OnRender(ViewProj, World->GetMainCamera()->GetCameraComponent()->GetLocation());
	Renderer->RenderAll(RenderQueue, ViewProj);
	FVector4 CamLoc = World->GetMainCamera()->GetCameraComponent()->GetLocation();
	if (Outline->GetTarget())
		OutlineRenderer->OnRender(*Outline, ViewProj, CamLoc);

	if (Gizmo->GetTarget())
	{ 
		auto Target = Gizmo->GetTarget()->GetOwner();
		FString Text = "UUID : " + std::to_string(Target->GetUUID());
		//FString Text = "FName : " + Target->GetName().ToString();
		Renderer->SetDepthStencilEnabled(false);
		TextRenderer->OnRender(Text, Gizmo->GetLocation(), 1.0f, *Atlas, World->GetMainCamera()->GetCameraComponent());
		Renderer->SetDepthStencilEnabled(true);
		GizmoRenderer->OnRender(*Gizmo, ViewProj);
	}

	ImGuiRenderer->Begin();
	EditorUI->OnRender();
	ImGuiRenderer->End();

	Renderer->EndFrame();
}

void Engine::Shutdown()
{
	UAssetManager::Get().Shutdown();
	for (int32 i = GUObjectArray.Num() - 1; i >= 0; --i)
		delete GUObjectArray[i];
	ImGuiRenderer->Shutdown();
	Renderer->Shutdown();
}

void Engine::HandleWindowResize()
{
	if (MainWindow->CheckResized())
	{
		Renderer->Resize(MainWindow->GetWidth(), MainWindow->GetHeight());
		World->GetMainCamera()->GetCameraComponent()->SetAspectRatio((float)MainWindow->GetWidth() / MainWindow->GetHeight());
	}
}

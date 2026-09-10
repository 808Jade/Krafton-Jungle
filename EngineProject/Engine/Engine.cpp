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
#include "Engine/ResourceManager.h"



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


	LOG(Info, "Initialize ResourceManager...");
	FResourceManager::GetInstance().Init(Renderer.get());
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

	// PropertyPanel Add
	PropertyPanel = EditorUI->AddEditorPanel<FPropertyPanel>();
	ControlPanel = EditorUI->AddEditorPanel<FControlPanel>();

	// Resource Manager 

	// OutLine
	OutlineRenderer = MakeUnique<FOutlineRenderer>();
	OutlineRenderer->Init(Renderer.get());

	Outline = MakeUnique<FOutline>();

	// Do Sth
	World = FObjectFactory::ConstructObject<UWorld>();
	World->Init();	//return bool

	FTransform Transform;
	AActor* Actor = World->SpawnActor(AActor::StaticClass(), &Transform);
	Actor->AddPrimitiveComponent(EPrimitiveType::Cube, Transform);

	FMeshData Data = FGeometryGenerator::CreateCube(1.0f);
	//FMeshData Data = FGeometryGenerator::CreateCylinder(1.0f, 3.0f, 20, FVector4(1.0f, 0.0f, 0.0f, 1.0f));
	//FMeshData Data = FGeometryGenerator::CreateCone(1.0f, 3.0f, 20, FVector4(1.0f, 0.0f, 0.0f, 1.0f));
	//FMeshData Data = FGeometryGenerator::CreateArrow(0.1f, 1.0f, 0.2f, 0.5f, 20, FVector4(1.0f, 0.0f, 0.0f, 1.0f));
	vb = Renderer->CreateVertexBuffer(Data.Vertices.data(), sizeof(FVertex) * (UINT)Data.Vertices.size(), sizeof(FVertex));
	ib = Renderer->CreateIndexBuffer(Data.Indices.data(), Data.Indices.size());

	Mesh = MakeShared<FMesh>();
	Mesh->VertexBuffer = vb;
	Mesh->IndexBuffer = ib;

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION" , 0 , DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	Shader = Renderer->CreateShader(L"Shader/DefaultShader.hlsl", layout, 2);

	Actor->GetPrimitiveComponent()->SetMeshShader(Mesh.get(), Shader.get());
	Actor->GetPrimitiveComponent()->SetMeshData(Data);

	PropertyPanel->FPropertyPanel::World = World;
	ControlPanel->FControlPanel::World = World;
	ControlPanel->FControlPanel::Mesh = Mesh;
	ControlPanel->FControlPanel::Shader = Shader.get();
	ControlPanel->SetGizmo(Gizmo.get());

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

	FMatrix Mat;
	Mat.SetIdentity();

	UPrimitiveComponent* OutlineComponent = nullptr;

	while (bIsRunning)
	{
		EngineTimer::Tick();
		float DeltaTime = EngineTimer::GetDeltaTime();
		ControlPanel->FControlPanel::DeltaTime = DeltaTime;

		//Check Swapchain Resize
		MainWindow->ProcessMessage(bIsRunning);
		if (MainWindow->CheckResized())
		{
			Renderer->Resize(MainWindow->GetWidth(), MainWindow->GetHeight());
			World->GetMainCamera()->GetCameraComponent()->SetAspectRatio((float)MainWindow->GetWidth() / MainWindow->GetHeight());
		}

		//Update World
		World->Tick(DeltaTime);

		EditorUI->Tick(DeltaTime);

		FMatrix VP = World->GetMainCamera()->GetCameraComponent()->GetViewProjectionMatrix();

		FRay ray = World->GetMainCamera()->GetCameraComponent()->DeProjection(FInputSystem::GetMouseX(), FInputSystem::GetMouseY(), MainWindow->GetWidth(), MainWindow->GetHeight());
		FVector2 mousePos(FInputSystem::GetMouseX(), FInputSystem::GetMouseY());
		bool bMouseDown = FInputSystem::IsMouseDown(EMouseButton::Left);

		Gizmo->Update(ray, mousePos, VP, MainWindow->GetWidth(), MainWindow->GetHeight(), bMouseDown, World->GetMainCamera()->GetCameraComponent());

		if (FInputSystem::IsMousePressed(EMouseButton::Left) && !Gizmo->IsUsing() && Gizmo->GetHoveredAxis() < 0 && !ImGui::GetIO().WantCaptureMouse)
		{
			UPrimitiveComponent* PickedComponent = World->GetPickingPrimitive(MainWindow->GetWidth(), MainWindow->GetHeight());
			OutlineComponent = PickedComponent;
			Gizmo->SetTarget(PickedComponent);
			Outline->SetTarget(OutlineComponent);
			PropertyPanel->SetTarget(PickedComponent);
		}



		TQueue<FRenderPacket> RenderQueue;
		World->GatherRenderPackets(RenderQueue);
		FInputSystem::UpdateInputStates();

		//BeginRendering
		Renderer->BeginFrame();
		Renderer->BindShader(Shader.get());
		GridRenderer->OnRender(VP, World->GetMainCamera()->GetCameraComponent()->GetLocation());
		Renderer->RenderAll(RenderQueue, VP);
		FVector4 CamLoc = World->GetMainCamera()->GetCameraComponent()->GetLocation();
		if (Outline->GetTarget())
			OutlineRenderer->OnRender(*Outline, VP, CamLoc);

		if (Gizmo->GetTarget())
		{
			Renderer->SetDepthStencilEnabled(false);
			GizmoRenderer->OnRender(*Gizmo, VP);
		}

		ImGuiRenderer->Begin();

		EditorUI->OnRender();

		ImGuiRenderer->End();

		Renderer->EndFrame();
	}
}

void Engine::Shutdown()
{
	for (UObject* Object : GUObjectArray)
		delete Object;
	ImGuiRenderer->Shutdown();
	Renderer->Shutdown();
}

void Engine::OnWindowResized(uint32 Width, uint32 Height)
{
	bIsResized = true;
}

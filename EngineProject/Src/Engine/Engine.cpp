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
#include "Render/RenderResourceManager.h"

#include "Render/RenderCommand.h"

bool Engine::Init(HINSTANCE hInstance)
{
	EditorUI = MakeUnique<FEditorUI>();
	EditorUI->Init();

	ConsolePanel = EditorUI->AddEditorPanel<FConsolePanel>();
	ConsolePanel->AddLog(ELogVerbosity::Info, "Engine Initialize...");

	LOG(Info, "Initialize Renderer...");
	Renderer = MakeUnique<FRenderer>();
	RenderDevice = MakeUnique<FRenderDevice>();
	RenderCommand::Init(RenderDevice.get());
	Renderer->Init();

	// Create Main Window & Swapchain
	FWindowContext MainWindowCtx;
	LOG(Info, "Create Main Window...");
	MainWindowCtx.Window = MakeUnique<FWindow>();
	if (!MainWindowCtx.Window->Create(hInstance, 1920, 1080, L"Hitori Engine"))
	{
		LOG(Error, "Failed To Create Main Window!");
		return false;
	}
	LOG(Info, "Success!");
	MainWindowCtx.Swapchain = MakeUnique<FSwapchain>(RenderDevice.get(), MainWindowCtx.Window.get());
	MainWindow = MainWindowCtx.Window.get();
	MainWindowSC = MainWindowCtx.Swapchain.get();
	Windows.Add(std::move(MainWindowCtx));


	LOG(Info, "Initialize AssetManager...");
	UAssetManager::Get().Init();
	LOG(Info, "Initialize AssetManager Success!");


	LOG(Info, "Initialize ImGui...");
	ImGuiRenderer = MakeUnique<FImGuiRenderer>();
	if (!ImGuiRenderer->Init(MainWindow->GetHandle(), RenderDevice->GetDevice(), RenderDevice->GetContext()))
	{
		LOG(Error, "Failed To Initialize ImGui!");
	}
	LOG(Info, "Initialize ImGui Success!");

	GridRenderer = MakeUnique<FGridRenderer>();
	GridRenderer->Init(Renderer.get());

	GizmoRenderer = MakeUnique<FGizmoRenderer>();
	GizmoRenderer->Init(Renderer.get());

	Gizmo = MakeUnique<FGizmo>();

	// 필요한 Panel들 추가후 raw pointer 반환(소유권 = EditorUI)
	PropertyPanel = EditorUI->AddEditorPanel<FPropertyPanel>();
	ControlPanel = EditorUI->AddEditorPanel<FControlPanel>();
	ViewportPanel = EditorUI->AddEditorPanel<FViewportPanel>();
	AssetBrowser = EditorUI->AddEditorPanel<FAssetBrowserPanel>();

	// OutLine
	OutlineRenderer = MakeUnique<FOutlineRenderer>();
	OutlineRenderer->Init(Renderer.get());

	ConfigPanel = EditorUI->AddEditorPanel<FConfigPanel>();

	Outline = MakeUnique<FOutline>();

	Atlas = UAssetManager::Get().LoadFontAtlas("Assets/Fonts/atlas.json", "Assets/Fonts/atlas.png");

	TextRenderer = MakeUnique<FTextRenderer>();
	TextRenderer->Init(Renderer.get());

	// Scene
	World = FObjectFactory::ConstructObject<UWorld>();
	World->Init();

	/// 삭제 예정
	//SceneManager = EditorUI->AddEditorPanel<FSceneManager>();
	//SceneManager->SetWorld(World);

	SceneOutliner = EditorUI->AddEditorPanel<FSceneOutliner>();
	SceneOutliner->SetWorld(World);
	SceneOutliner->SetSelectionCallback(
		[this](UPrimitiveComponent* Primitive)
		{
			Gizmo->SetTarget(Primitive);
			Outline->SetTarget(Primitive);
			PropertyPanel->SetTarget(Primitive);
		}
	);

	LineBatcher = MakeUnique<FLineBatcher>();
	LineBatcher->Init(Renderer.get(), World);

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

	while (bIsRunning)
	{
		EngineTimer::Tick();
		float DeltaTime = EngineTimer::GetDeltaTime();
		ControlPanel->FControlPanel::DeltaTime = DeltaTime;

		FInputSystem::UpdateInputStates();

		//Check Swapchain Resize
		MainWindow->ProcessMessage(bIsRunning);
		if (!bIsRunning)
			break;

		//메시지 처리 직후 리사이즈 검사
		HandleMainWindow();

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
	FRay ray = World->GetMainCamera()->GetCameraComponent()->DeProjection(ViewportPanel->GetLocalMousePos(), ViewportPanel->GetWidth(), ViewportPanel->GetHeight());
	bool bMouseDown = FInputSystem::IsMouseDown(EMouseButton::Left);

	Gizmo->Update(ray, ViewportPanel->GetLocalMousePos(), ViewProj, ViewportPanel->GetWidth(), ViewportPanel->GetHeight(), bMouseDown, World->GetMainCamera()->GetCameraComponent());

	if (FInputSystem::IsMousePressed(EMouseButton::Left) && !Gizmo->IsUsing() && Gizmo->GetHoveredAxis() < 0 && ViewportPanel->IsHovered())
	{
		//LOG(Info, "Mouse Pos {}, {}", ViewportPanel->GetLocalMousePos().X, ViewportPanel->GetLocalMousePos().Y)
		UPrimitiveComponent* PickedComponent = World->TryPicking(ViewportPanel->GetLocalMousePos(), ViewportPanel->GetWidth(), ViewportPanel->GetHeight());
		Gizmo->SetTarget(PickedComponent);
		Outline->SetTarget(PickedComponent);
		PropertyPanel->SetTarget(PickedComponent);
	}
}

void Engine::RenderFrame(FMatrix& ViewProj, TQueue<FRenderPacket>& RenderQueue)
{
	RenderCommand::BeginRenderPass(ViewportPanel->GetViewportRenderingInfo());

	if (ConfigPanel->GetSettings().bDrawBatchLine)
	{
		if (ConfigPanel->GetSettings().bDrawBoundingBox)
		{
			LineBatcher->BeginFrame();
			LineBatcher->BuildVertexBuffer();
			World->GetPathTracker().OnRender(LineBatcher.get());
			LineBatcher->OnRender(ViewProj);
		}

		if (ConfigPanel->GetSettings().bDrawPSGrid)
		{
			GridRenderer->OnRenderPSGrid(
				ViewProj, World->GetMainCamera()->GetCameraComponent()->GetWorldLocation(), ConfigPanel->GetSettings()
			);
		}
		else
		{
			GridRenderer->OnRenderBatchGrid(
				ViewProj, World->GetMainCamera()->GetCameraComponent()->GetWorldLocation()
			);
		}
	}

	if (ConfigPanel->GetSettings().bDrawPrimitives)
	{
		RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
		RenderCommand::SetBlendState(EBlendState::Opaque);
		RenderCommand::SetDepthStencilState(EDepthStencilState::Default);

		if (ConfigPanel->GetSettings().bWireframe)
		{
			RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		}
		else
		{
			RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		}

		Renderer->RenderAll(RenderQueue, World->GetMainCamera()->GetCameraComponent());
	}

	// TextRenderComponent 렌더링
	for (UTextRenderComponent* TextComponent : World->GetTextComponents())
	{
		if (!TextComponent || !TextComponent->GetFont() || !TextComponent->IsVisible())
		{
			continue;
		}

		TextRenderer->OnRender(
			TextComponent->GetText(),
			TextComponent->GetWorldMatrix(),
			TextComponent->GetTextSize(),
			*TextComponent->GetFont(),
			World->GetMainCamera()->GetCameraComponent()
		);
	}

	FVector4 CamLoc =
		World->GetMainCamera()->GetCameraComponent()->GetWorldLocation();

	if (Outline->GetTarget())
	{
		OutlineRenderer->OnRender(*Outline, ViewProj, CamLoc);
	}

	if (Gizmo->GetTarget())
	{
		auto Target = Cast<UPrimitiveComponent>(Gizmo->GetTarget());
		FBox box = Target->GetMeshData().AABB.GetWorldAABB(Target->GetWorldMatrix());

		RenderCommand::ClearDepthStencil(ViewportPanel->GetViewportRenderingInfo().DepthSteincil.Texture);
		GizmoRenderer->OnRender(*Gizmo, ViewProj);

		RenderCommand::ClearDepthStencil(ViewportPanel->GetViewportRenderingInfo().DepthSteincil.Texture);
		FString Text = "UUID : " + std::to_string(Target->GetUUID());

		if (ConfigPanel->GetSettings().bShowUUID)
		{
			float Padding = 0.5f;

			FVector UUIDLocation;
			UUIDLocation.X = (box.Min.X + box.Max.X) * 0.5f;
			UUIDLocation.Y = (box.Min.Y + box.Max.Y) * 0.5f;

			UUIDLocation.Z = box.Max.Z + Padding;

			TextRenderer->BuildTextMesh(Text, 0.5f, *Atlas);
			TextRenderer->OnRenderBillboard(
				Text,
				UUIDLocation,
				0.5f,
				*Atlas,
				World->GetMainCamera()->GetCameraComponent()
			);
		}
	}


	RenderCommand::EndRenderPass(ViewportPanel->GetViewportRenderingInfo());

	// Swapchain 렌더링
	RenderCommand::BeginRenderPass(MainWindowSC->GetRenderingInfo());

	ImGuiRenderer->Begin();

	ControlPanel->DrawMainMenuBar();
	EditorUI->OnRender();

	ImGuiRenderer->End();

	RenderCommand::EndRenderPass(MainWindowSC->GetRenderingInfo());

	MainWindowSC->SwapBuffers();
}

void Engine::Shutdown()
{
	UAssetManager::Get().Shutdown();
	FRenderResourceManager::Shutdown();
	for (int32 i = GUObjectArray.Num() - 1; i >= 0; --i)
		delete GUObjectArray[i];
	ImGuiRenderer->Shutdown();
	RenderDevice->Shutdown();
}

void Engine::HandleMainWindow()
{
	if (MainWindow->CheckResized())
	{
		MainWindowSC->Resize(MainWindow->GetWidth(), MainWindow->GetHeight());
	}
	World->GetMainCamera()->GetCameraComponent()->SetAspectRatio(ViewportPanel->GetViewportAspectRatio());
}

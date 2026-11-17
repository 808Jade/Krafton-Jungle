#include "EnginePCH.h"

#include "Editor/HitoriEd/EditorEngine.h"

#include "Core/EngineStatics.h"
#include "Core/EngineTimer.h"
#include "Launch/LaunchEngineLoop.h"
#include "Core/StatOverlay.h"
#include "Input/InputSystem.h"


#include "Render/GeometryGenerator.h"

#include "Engine/World.h"
#include "Engine/Level.h"

#include "Render/Renderer.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
// 피킹 결과와 View별 Billboard 행렬 공급에 사용한다.
#include "Collision/HitResult.h"
#include "Components/BillboardComponent.h"
#include "GameFramework/Actor/LightActorBase.h"
#include "GameFramework/Actor/DecalActor.h"

#include "Asset/AssetManager.h"
#include "Render/RenderResourceManager.h"

#include "Render/RenderCommand.h"
#include "Editor/Outliner/OutlinerPanel.h"
#include "Editor/Sequence/SequencePanel.h"
#include "Editor/SkeletalMeshViewer/SkeletalMeshViewerTab.h"
#include "Editor/LevelEditor/LevelEditorTab.h"
#include "Editor/ParticleSystem/ParticleSystemEditorTab.h"
#include "Particles/ParticleSystem.h"

#include "Editor/BlendSpaceEditor/BlendSpaceEditorPanel.h"
#include "Editor/StateMachineEditor/StateMachineEditorPanel.h"
#include "Editor/AnimGraphEditor/AnimGraphEditorPanel.h"
#include "Animation/AssetAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UObjectIterator.h"
#include "Sequence/SequenceActor.h"
#include "Editor/HitoriEd/EditorFileUtils.h"
#include "Editor/HitoriEd/EditorResourcePaths.h"
#include "Core/ResourcePaths.h"
#include "UObject/UObjectIterator.h"

#include "Core/EngineLog.h"
#include "Core/Stats/LightweightStats.h"

// 임시 Shadow 연결을 위한 include
// TODO : FSceneRenderer 도입 후 옮겨야함
#include "ProjectedShadowInfo.h"
#include <utility>
#include <algorithm>
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLightSceneProxy.h"
#include "Engine/SceneTransitionManager.h"
#include "Core/Stats/ShadowStats.h"
#include "Math/Frustum.h"

#include "Core/CoroutineScheduler.h"
#include <sol/sol.hpp>
#include "Components/ShapeComponent.h"

namespace
{
	DECLARE_CYCLE_STAT("Viewport Update", STAT_ViewportUpdate);
	DECLARE_CYCLE_STAT("World Tick", STAT_WorldTick);
	DECLARE_CYCLE_STAT("Editor Tick", STAT_EditorTick);
	DECLARE_CYCLE_STAT("Prepare Render Views", STAT_PrepareRenderViews);
	DECLARE_CYCLE_STAT("Build Render Queue", STAT_BuildRenderQueue);
	DECLARE_CYCLE_STAT("ImGui", STAT_ImGui);
}

#include "Core/SplashScreen.h"

FEngineConfig UEditorEngine::GetConfig() const
{
	FEngineConfig Desc;
	Desc.Title = L"Hitori Engine";
	Desc.Width = 1920;
	Desc.Height = 1080;
	Desc.bBorderless = false;
	Desc.SyncInterval = 0;
	// View는 각자 깊이 버퍼를 쓰고 백버퍼에는 ImGui만 그린다.
	Desc.bCreateDepthBuffer = false;
	Desc.bExitOnEscape = false;
	// 파일이 없으면 검은 배경에 상태 텍스트만 표시된다.
	Desc.SplashImage = ResourcePaths::SplashImage;
	return Desc;
}

// 렌더 자원·월드·에디터와 MultipleViewports 연결을 초기화한다.
// Device·Window·Swapchain·AssetManager는 FEngineLoop가 먼저 만들어 둔다.
bool UEditorEngine::Init()
{
	if (!Super::Init())
		return false;

	FWorldContext& InitialWorldContext = CreateNewWorldContext(EWorldType::Editor);
	InitialWorldContext.SetCurrentWorld(UWorld::CreateWorld(EWorldType::Editor, true));
	EditorWorld = InitialWorldContext.World();

	MainWindow = GetEngineLoop().GetMainWindow();
	MainWindowSC = GetEngineLoop().GetSwapchain();
	Renderer = GetEngineLoop().GetRenderer();
	FRenderDevice* RenderDevice = GetEngineLoop().GetRenderDevice();

	EditorUI = MakeUnique<FEditorUI>();
	EditorUI->Init();

	EditorUI->SetNewSceneCallback([this]() { CreateNewScene(); });
	EditorUI->SetOpenSceneCallback([this]() { OpenScene(); });
	EditorUI->SetSaveSceneCallback([this]() { SaveCurrentScene(); });
	EditorUI->SetSaveSceneAsCallback([this]() { SaveSceneAs(); });

	OutputLogPanel = EditorUI->GetLevelTab()->AddPanel<FOutputLogPanel>();
	FLog::AddSink(OutputLogPanel);

	// 로그 패널이 생긴 뒤에 돌려야 결과가 Output Log에 보인다.
	OutputLogPanel->SetCommandHandler(
		[this](const FString& CommandLine, FString& OutMessage)
		{
			if (ExecShadowCommand(CommandLine, OutMessage))
				return true;

			if (ExecShadowResolutionCommand(CommandLine, OutMessage))
				return true;

			return false;
		}
	);
	HTR_LOG(Info, "Editor Initialize...");

	HTR_LOG(Info, "Initialize ImGui...");
	ImGuiRenderer = MakeUnique<FImGuiRenderer>();
	if (!ImGuiRenderer->Init(MainWindow->GetHandle(), RenderDevice->GetDevice(), RenderDevice->GetContext()))
	{
		HTR_LOG(Error, "Failed To Initialize ImGui!");
	}
	HTR_LOG(Info, "Initialize ImGui Success!");

	GridRenderer = MakeUnique<FGridRenderer>();
	GridRenderer->Init(Renderer);

	GizmoRenderer = MakeUnique<FGizmoRenderer>();
	GizmoRenderer->Init(Renderer);

	SkeletonRenderer = MakeUnique<FSkeletonRenderer>();
	SkeletonRenderer->Init();

	Gizmo = MakeUnique<FGizmo>();

	FogRenderer = MakeUnique<FFogRenderer>();
	FogRenderer->Init(Renderer);

	DecalRenderer = MakeUnique<FDecalRenderer>();
	DecalRenderer->Init(Renderer);

	ParticleSpriteRenderer = MakeUnique<FParticleSpriteRenderer>();
	ParticleSpriteRenderer->Init();

	ParticleMeshRenderer = MakeUnique<FParticleMeshRenderer>();
	ParticleMeshRenderer->Init();

	ParticleStripRenderer = MakeUnique<FParticleStripRenderer>();
	ParticleStripRenderer->Init();

	FXAARenderer = MakeUnique<FFXAARenderer>();
	FXAARenderer->Init();

	DepthViewRenderer = MakeUnique<FDepthViewRenderer>();
	DepthViewRenderer->Init();

	ShadowViewRenderer = MakeUnique<FShadowViewRenderer>();
	ShadowViewRenderer->Init();

	ToneMapRenderer = MakeUnique<FToneMapRenderer>();
	ToneMapRenderer->Init();


	// 레벨 패널은 레벨 탭이 소유하고 엔진은 raw pointer로 참조한다. (소유권 = 레벨 탭)
	DetailsPanel = EditorUI->GetLevelTab()->AddPanel<FDetailsPanel>();
	// 패널에는 조회 대상만 전달하고, 그림자 작업이나 타깃을 보관하지 않는다.
	DetailsPanel->SetRenderer(Renderer);

	DetailsPanel->SetShadowCascadeDebugCallbacks(
		[this](uint32 ViewId)
		{
			const FViewport* Viewport = FindLevelViewportByShadowId(ViewId);
			return Viewport && Viewport->GetClient().GetShowFlags().IsEnabled(EEngineShowFlag::ShadowCascades);
		},
		[this](uint32 ViewId, bool bEnabled)
		{
			FViewport* Viewport = FindLevelViewportByShadowId(ViewId);
			if (!Viewport)
				return;

			// 다른 View 플래그는 유지하고 색상 표시만 변경한다.
			FEngineShowFlags Flags = Viewport->GetClient().GetShowFlags();
			Flags.SetEnabled(EEngineShowFlag::ShadowCascades, bEnabled);
			Viewport->GetClient().SetShowFlags(Flags);
		});

	EditorControlsPanel = EditorUI->GetLevelTab()->AddPanel<FEditorControlsPanel>();
	ViewportsPanel = EditorUI->GetLevelTab()->AddPanel<FViewportsPanel>();
	ContentDrawerPanel = EditorUI->GetLevelTab()->AddPanel<FContentDrawerPanel>();
	BlendSpaceEditorPanel = EditorUI->AddEditorPanel<FBlendSpaceEditorPanel>();
 AnimGraphEditorPanel=EditorUI->AddEditorPanel<FAnimGraphEditorPanel>();
 StateMachineEditorPanel=EditorUI->AddEditorPanel<FStateMachineEditorPanel>();
 StateMachineEditorPanel->SetPIEDebugProvider([this](const FString &AssetKey, const FString &Signature) {
	 FStateMachinePIEFrame Frame;
	 if (!PlayWorld)
		 return Frame;
	 Frame.Session = PlayWorld->GetUUID();
	 if (AssetKey.empty())
		 return Frame;
	 for (TObjectIterator<AActor> It; It; ++It)
	 {
		 auto *Actor = *It;
		 if (Actor->GetWorld() != PlayWorld || Actor->IsActorBeingDestroyed())
			 continue;
		 for (auto *Component : Actor->GetComponents())
		 {
			 auto *Mesh = Cast<USkeletalMeshComponent>(Component);
			 if (!Mesh || StateMachineAssetKey(Mesh->GetStateMachineAssetPath()) != AssetKey)
				 continue;
			 auto *Instance = Cast<UAssetAnimInstance>(Mesh->GetAnimInstance());
			 if (!Instance)
				 continue;
			 FStateMachinePIETarget Target;
			 Target.ActorId = Actor->GetUUID();
			 Target.ComponentId = Mesh->GetUUID();
			 Target.Label = Actor->GetName() + " / " + Mesh->GetName();
			 Target.Observation = ObserveStateMachine(*Instance, Signature);
			 Frame.Targets.push_back(std::move(Target));
		 }
	 }
	 std::sort(Frame.Targets.begin(), Frame.Targets.end(), [](const auto &A, const auto &B) {
		 if (A.Label != B.Label)
			 return A.Label < B.Label;
		 return A.ActorId != B.ActorId ? A.ActorId < B.ActorId : A.ComponentId < B.ComponentId;
	 });
	 return Frame;
 });
 ContentDrawerPanel->SetOpenStateMachineCallback([this](const FString &P) { AnimGraphEditorPanel->OpenAsset(P); });
 ContentDrawerPanel->SetCreateStateMachineCallback([this](const FString& P){AnimGraphEditorPanel->NewStateMachine(P);});
	ContentDrawerPanel->SetOpenBlendSpaceCallback([this](const FString& Path) { BlendSpaceEditorPanel->OpenAsset(Path); });
	// 파티클 에디터는 에셋마다 탭 하나다. 렌더러는 알지 못하고, 프리뷰는 탭의 CollectViews로 RenderAllViews가 그린다.
	ContentDrawerPanel->SetOpenParticleSystemCallback([this](const FString& Path) { OpenParticleSystemFile(Path); });
	EditorUI->AddWindowMenuCommand("Open Particle System Editor", [this]()
	{
		OpenParticleSystemEditor(UAssetManager::GetAssetByPath<UParticleSystem>("DefaultParticleSystem"));
	});
	// 스켈레탈 뷰어는 메시마다 탭 하나다. (OpenSkeletalMeshViewer)
	// 아래 공유 뷰어는 State Machine·AnimGraph 에디터가 서비스로 쓰는 것이다. 미리보기를 띄우면 "Animation Preview" 탭이 이 뷰어를 빌려 보여 준다.
	AnimationPreviewTab = EditorUI->AddTab<FSkeletalMeshViewerTab>(FString("SkeletalAnimationPreview"));
	// 미리보기를 요청받기 전까지는 숨겨 둔다. 미리보기가 시작되면 탭이 스스로 다시 열린다. (RequestFocus)
	if (AnimationPreviewTab)
		AnimationPreviewTab->Close();
	EditorUI->AddWindowMenuCommand("Open Skeletal Mesh Viewer", [this]() { OpenSkeletalMeshViewer(nullptr); });
 StateMachineEditorPanel->SetViewer(AnimationPreviewTab);
 AnimGraphEditorPanel->SetViewer(AnimationPreviewTab);
 AnimGraphEditorPanel->SetStateMachineTemplate(StateMachineEditorPanel);
 StateMachineEditorPanel->OpenHost=[this](){AnimGraphEditorPanel->SetOpen(true);};
 ContentDrawerPanel->SetOpenAnimGraphCallback([this](const FString& P){ AnimGraphEditorPanel->OpenAsset(P); });
 ContentDrawerPanel->SetCreateAnimGraphCallback([this](const FString& P){ AnimGraphEditorPanel->NewAsset(P); });
	if (AnimationPreviewTab)
	AnimationPreviewTab->Animation.SetOpenBlendSpaceEditorCallback([this](const FString& Path) { BlendSpaceEditorPanel->OpenAsset(Path); });
	DetailsPanel->SetOpenSkeletalMeshViewerCallback([this](USkeletalMesh* Mesh) { OpenSkeletalMeshViewer(Mesh); });
	ContentDrawerPanel->SetOpenSkeletalMeshCallback([this](const FString& Path) { OpenSkeletalMeshFile(Path); });
	FSequencePanel* SequencePanel = EditorUI->GetLevelTab()->AddPanel<FSequencePanel>();
	SequencePanel->SetSequenceActorProvider([this]() -> ASequenceActor*
	{
		ASequenceActor* Actor = Cast<ASequenceActor>(OutlinerPanel->GetSelectedActor());
		return Actor && Actor->GetWorld() == EditorWorld && !Actor->IsActorBeingDestroyed() ? Actor : nullptr;
	});
	SequencePanel->SetCreateSequenceActorCallback([this]()
	{
		if (IsPlaying() || !EditorWorld) return;
		if (ASequenceActor* Actor = EditorWorld->SpawnActor<ASequenceActor>())
			OutlinerPanel->SelectActor(Actor);
	});
	SequencePanel->SetViewportCameraProvider([this](CameraKey& OutKey)
	{
		// 입력을 받는 View, 없으면 마지막 편집 View의 카메라를 쓴다.
		const FViewport* Viewport = ViewportsPanel->GetActiveViewport();
		if (!Viewport)
			Viewport = ViewportsPanel->GetEditorViewport();
		if (!Viewport || !Viewport->IsActive())
			return false;

		const FViewCamera& Camera = Viewport->GetEffectiveCamera();
		OutKey.Location = Camera.Transform.Location;
		OutKey.Rotation = Camera.Transform.Rotation.ToFRotator();
		OutKey.FieldOfView = Camera.Projection.FovDegrees;
		return true;
	});

	// OutLine
	OutlineRenderer = MakeUnique<FOutlineRenderer>();
	OutlineRenderer->Init(Renderer);

	SettingsPanel = EditorUI->GetLevelTab()->AddPanel<FSettingsPanel>();

	//ToolbarPanel->SetPlayCallback([this]() { RequestPlaySession(); });
	//ToolbarPanel->SetStopCallback([this]() { RequestEndPlayMap(); });
	//ToolbarPanel->SetIsPlayingQuery([this]() { return PlayWorld != nullptr; });

	Outline = MakeUnique<FOutline>();

	SystemFont = UAssetManager::GetAssetByPath<UFont>(ResourcePaths::DefaultFont);

	TextRenderer = MakeUnique<FTextRenderer>();
	TextRenderer->Init();

	UIRenderer = MakeUnique<FUIRenderer>();
	UIRenderer->Init(*TextRenderer);

	// 투영 행렬 생성 
	ViewportsPanel->InitializeFromWorld(*GetActiveWorld());
	// 화면 나눔 비율 설정 가져오기
	ViewportsPanel->SetSplitRatio({
		SettingsPanel->GetSettings().MultipleViewportsHorizontal,
		SettingsPanel->GetSettings().MultipleViewportsVertical });
	// SingleView에 사용할 인덱스 설정
	ViewportsPanel->SetSingleViewIndex(
		SettingsPanel->GetSettings().MultipleViewportsSingleViewIndex);
	// 뷰포트 레이아웃 설정
	ViewportsPanel->SetLayoutMode(
		SettingsPanel->GetSettings().bMultipleViewportsSingle
		? ELayoutMode::Single
		: ELayoutMode::QuadSplit);
	GetActiveWorld()->GetMainCameraComponent()->SetExternalInputManaged(true);

	/// 삭제 예정
	//SceneManager = EditorUI->AddEditorPanel<FSceneManager>();
	//SceneManager->SetWorld(World);

	OutlinerPanel = EditorUI->GetLevelTab()->AddPanel<FOutlinerPanel>();
	OutlinerPanel->SetWorld(GetActiveWorld());
	OutlinerPanel->SetSelectionCallback(
		[this](USceneComponent* Root)
		{
			Gizmo->SetTarget(Root, true);
			DetailsPanel->SetTarget(Root ? Root->GetOwner() : nullptr);

			UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Root);
			if (!Primitive && Root && Root->GetOwner())
			{
				for (UActorComponent* C : Root->GetOwner()->GetComponents())
					if ((Primitive = Cast<UPrimitiveComponent>(C))) break;
			}
			Outline->SetTarget(Primitive);
		});
	DetailsPanel->SetSelectionCallback(
		[this](UActorComponent* Component)
		{
			if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
			{
				AActor* Owner = SceneComponent->GetOwner();
				if (Owner && SceneComponent == Owner->GetRootComponent())
				{
					Gizmo->SetTarget(SceneComponent, true);
				}
				else
				{
					Gizmo->SetTarget(SceneComponent);
				}
			}
			else
			{
				Gizmo->SetTarget(nullptr);
			}
		}
	);

	OutlinerPanel->SetDeleteActorCallback(
		[this](AActor* Actor)
		{
			DeleteActor(Actor);
		}
	);
	DetailsPanel->SetDeleteComponentCallback(
		[this](UActorComponent* Component)
		{
			DeleteComponent(Component);
		}
	);
	DetailsPanel->SetMakeRootCallback(
		[this](USceneComponent* NewRoot)
		{
			MakeRootComponent(NewRoot);
		}
	);

	LineBatcher = MakeUnique<FLineBatcher>();
	LineBatcher->Init(Renderer, GetActiveWorld());

	DetailsPanel->SetWorld(GetActiveWorld());

	EditorControlsPanel->SetWorld(GetActiveWorld());
	EditorControlsPanel->SetIsPlayingQuery([this]() { return IsPlaying(); });
	EditorControlsPanel->SetGizmo(Gizmo.get());
	EditorControlsPanel->SetViewportsPanel(ViewportsPanel);

	SettingsPanel->SetWorld(GetActiveWorld());
	SettingsPanel->SetViewportsPanel(ViewportsPanel);
	// 레벨 탭은 자기 View 넷을 엔진의 공통 렌더 경로(RenderAllViews)에 넘긴다.
	EditorUI->GetLevelTab()->SetViewCollector([this](TArray<FEditorTabView>& OutViews) { CollectLevelViews(OutViews); });

	SkyboxRenderer = MakeUnique<FSkyboxRenderer>();
	SkyboxRenderer->Init(ResourcePaths::DefaultSkyPanorama);

	const ImVec4 White = { 1, 1, 1, 1 };
	const ImVec4 Green = { 0.40f, 0.80f, 0.30f, 1.0f };

	// Resources는 시작할 때 스캔하지 않으므로 툴바 아이콘은 경로로 직접 불러온다.
	// Save 아이콘 파일은 아직 없어서 아이콘 없이 둔다 (이전에도 찾지 못해 nullptr였다).
	Save = { "Save",   "Save Current Scene",nullptr,  White, { EKeyCode::S, true } };

	Play = { "Play",   "Play World",        UAssetManager::Get().LoadTexture(EditorResourcePaths::ToolbarPlayIcon),  Green, { EKeyCode::P, false, true } };

	Play.CanExecute = [this]()->bool {return !PlayWorld; };
	Play.Execute = [this]() { RequestPlaySession(); };

	Pause = { "Pause",  "Pause",			UAssetManager::Get().LoadTexture(EditorResourcePaths::ToolbarPauseIcon), White, { EKeyCode::Pause } };

	Pause.CanExecute = [this]()->bool {return !bIsPaused && PlayWorld; };
	Pause.Execute = [this]() { bIsPaused = true; };


	Resume = { "Resume", "Resume",          UAssetManager::Get().LoadTexture(EditorResourcePaths::ToolbarPlayIcon),  White, { EKeyCode::Pause } };

	Resume.CanExecute = [this]()->bool {return bIsPaused && PlayWorld; };
	Resume.Execute = [this]() { bIsPaused = false; };

	StepFrame = { "Step", "Step One Frame", UAssetManager::Get().LoadTexture(EditorResourcePaths::ToolbarStepIcon),  White, {} };

	StepFrame.CanExecute = [this]()->bool {return bIsPaused && PlayWorld; };
	StepFrame.Execute = [this]() { bStepRequested= true; };


	Stop = { "Stop",   "Stop",				UAssetManager::Get().LoadTexture(EditorResourcePaths::ToolbarStopIcon),  White, { EKeyCode::Escape } };

	Stop.CanExecute = [this]()->bool {return PlayWorld; };
	Stop.Execute = [this]() { RequestEndPlayMap(); };

	EditorUI->GetToolbar()->SetCommands({ Play, Pause, Resume, StepFrame, Stop });
	EditorUI->GetToolbar()->SetPlayModeBinding(
		[this]() { return PlayMode; },
		[this](EPlayModeType Mode) { PlayMode = Mode; },
		[this]() { return !IsPlaying(); });

	return true;
}

// 프레임 시작·View 상태·월드 갱신 후 View별 오프스크린 렌더와 UI 합성을 진행한다.
// 입력·창 메시지는 FEngineLoop가 먼저 처리하고 Present는 호출 직후에 한다.
void UEditorEngine::Tick(const float DeltaTime)
{
	// PIE 창의 닫기 버튼은 앱이 아니라 PIE 세션만 끝낸다.
	if (PIEWindow && PIEWindow->ConsumeCloseRequest())
		RequestEndPlayMap();
	if (bRequestEndPlayMapQueued) { bRequestEndPlayMapQueued = false; EndPlayMap(); }
	if (bPlaySessionRequested) { bPlaySessionRequested = false; if (!PlayWorld) StartPlayInEditorSession(); }

	ImGuiIO& IO = ImGui::GetIO();
	if (IsPlaying() && !bIsPaused)
		IO.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
	else
		IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	BeginFrame(DeltaTime);
	// 임시: 선택된 Actor를 복제해서 결과를 로그로 확인 (PIE 연결 후 삭제)
	if (!IsPlaying() && !ImGui::GetIO().WantTextInput && FInputSystem::IsKeyPressed(EKeyCode::F9))
	{
		UWorld* PIEWorld = UWorld::CreateWorld(EWorldType::PIE, false);
		FObjectDuplicationParameters Params(EditorWorld, nullptr);
		Params.DuplicationSeed.Add(EditorWorld, PIEWorld);
		Params.DuplicationSeed.Add(EditorWorld->GetPersistentLevel(), PIEWorld->GetPersistentLevel());
		Params.PortFlags = EPropertyPortFlags::PPF_DuplicateForPIE;
		StaticDuplicateObjectEx(Params);

		HTR_LOG(Info, "[DupTest] actors {} -> {}",
			EditorWorld->GetPersistentLevel()->GetActors().Num(),
			PIEWorld->GetPersistentLevel()->GetActors().Num());
		for (AActor* A : PIEWorld->GetPersistentLevel()->GetActors())
			HTR_LOG(Info, "[DupTest]   {} world={} level={} root owner ok={}",
				A->GetName(), A->GetWorld() == PIEWorld, A->GetLevel() == PIEWorld->GetPersistentLevel(),
				A->GetRootComponent() && A->GetRootComponent()->GetOwner() == A);
	}
	UpdateMultipleViewportState(DeltaTime);
	UpdatePIEViewCamera();
	TickWorldAndEditor(DeltaTime);
	if (PlayWorld)
	{
		if (FSceneTransitionManager* SceneTransitions = GetSceneTransitionManager())
			SceneTransitions->ProcessPendingTransition();
	}
	RenderAllViews();
	EndFrame();
}

// 기존 Directional 설정을 변경하여 다음 프레임의 그림자 준비에 반영한다.
bool UEditorEngine::ExecShadowCommand(const FString& CommandLine, FString& OutMessage)
{
	std::istringstream Input(CommandLine);
	FString Command, Action, Value, Extra;
	Input >> Command >> Action >> Value >> Extra;

	// 다른 명령은 기존 콘솔 처리 경로에서 계속 판정한다.
	if (_stricmp(Command.c_str(), "CSM") != 0)
		return false;

	// 잘못된 입력은 아래 검증에서 설정 변경 없이 이 메시지를 반환한다.
	OutMessage = "Usage: CSM | CSM ON | CSM OFF | CSM SET <0-4>";
	if (!Extra.empty())
		return true;

	// -1은 개수 변경이 없는 토글·ON·OFF 명령을 의미한다.
	int32 RequestedCount = -1;
	if (_stricmp(Action.c_str(), "SET") == 0)
	{
		// 현재 지원 범위인 한 자리 정수 0~4만 허용한다.
		if (Value.size() != 1 || Value[0] < '0'	|| Value[0] > '0' + UDirectionalLightComponent::MaxShadowCascades)
			return true;

		RequestedCount = Value[0] - '0';
	}
	else
	{
		// SET 이외에는 인자가 없거나 ON/OFF만 사용할 수 있다.
		if (!Value.empty() || (!Action.empty() && _stricmp(Action.c_str(), "ON") != 0 && _stricmp(Action.c_str(), "OFF") != 0))
			return true;
	}

	// 평소에는 EditorWorld, PIE 중에는 PlayWorld를 대상으로 한다.
	UWorld* World = GetActiveWorld();
	const FDirectionalLightSceneProxy* Primary = World->GetScene().GetPrimaryDirectionalLight();

	if (!Primary)
	{
		OutMessage = "No active Directional Light in the current world.";
		return true;
	}

	// 실제 Primary의 Owner에서 수정 가능한 동일 Component를 찾는다.
	const ULightComponent* PrimaryComponent = Primary->GetLightComponent();
	UDirectionalLightComponent* Light = nullptr;
	for (UActorComponent* Component : PrimaryComponent->GetOwner()->GetComponents())
	{
		if (Component == PrimaryComponent)
		{
			Light = Cast<UDirectionalLightComponent>(Component);
			break;
		}
	}

	if (!Light)
	{
		OutMessage = "Directional Light component was not found.";
		return true;
	}

	if (RequestedCount >= 0)
	{
		// SET 0은 마지막 개수를 보존하며 CSM만 비활성화한다.
		if (RequestedCount > 0)
			Light->SetCascadeCount(RequestedCount);

		Light->SetCSMEnabled(RequestedCount > 0);
	}
	else if (Action.empty())
	{
		// 단독 CSM 입력은 기존 STAT 명령처럼 현재 상태를 반전한다.
		Light->SetCSMEnabled(!Light->IsCSMEnabled());
	}
	else
	{
		// 입력 검증을 통과했으므로 남은 Action은 ON 또는 OFF다.
		Light->SetCSMEnabled(_stricmp(Action.c_str(), "ON") == 0);
	}

	// OFF에서는 출력된 개수가 저장된 설정값임을 명시한다.
	const bool bEnabled = Light->IsCSMEnabled();
	OutMessage = std::format(
		"CSM: {}, {}: {} (World: {})",
		bEnabled ? "ON" : "OFF",
		bEnabled ? "Cascades" : "Saved Cascades",
		Light->GetCascadeCount(),
		World->IsPlayInEditor() ? "PIE" : "Editor"
	);
	return true;
}

bool UEditorEngine::ExecShadowResolutionCommand(const FString& CommandLine, FString& OutMessage)
{
	std::istringstream Input(CommandLine);
	FString Command, Extra;
	Input >> Command;

	// 다른 명령은 기존 콘솔 처리 경로에서 계속 판정한다.
	if (_stricmp(Command.c_str(), "ShadowResolution") != 0)
		return false;

	// 잘못된 입력은 아래 검증에서 설정 변경 없이 이 메시지를 반환한다.
	OutMessage = "Usage: ShadowResolution <128 | 256 | 512 | 1024 | 2048 | 4096>";
	int32 Resolution = 0;
	if (!(Input >> Resolution) || (Input >> Extra) || !Renderer->SetShadowMapResolution(Resolution))
		return true;
	OutMessage = std::format("ShadowMap global maximum: {}", Resolution);

	return true;
}

// DeltaTime을 패널에 전달하고 에디터 단축키를 처리한다.
void UEditorEngine::BeginFrame(const float DeltaTime)
{
	FStatOverlay::Tick(DeltaTime);
	EditorControlsPanel->FEditorControlsPanel::DeltaTime = DeltaTime;

	if (!AnimGraphEditorPanel->IsKeyboardFocused() && !StateMachineEditorPanel->IsKeyboardFocused() && !BlendSpaceEditorPanel->IsKeyboardFocused() && !ImGui::GetIO().WantTextInput && FInputSystem::IsKeyPressed(EKeyCode::Delete))
		DeleteActor(OutlinerPanel->GetSelectedActor());
}

// 패널의 Layout·Preset 요청과 입력을 레벨 View에 반영한다.
void UEditorEngine::UpdateMultipleViewportState(const float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ViewportUpdate);

	const FVector2 ViewportSize = ViewportsPanel->GetContentSize();
	const FVector2 LocalMousePosition = ViewportsPanel->GetLocalMousePosition();

	ELayoutMode RequestedLayout{};
	int32 RequestedSingleViewIndex = ViewportsPanel->GetSingleViewIndex();
	if (ViewportsPanel->ConsumeLayoutRequest(RequestedLayout, RequestedSingleViewIndex))
	{
		if (RequestedLayout == ELayoutMode::Single)
			ViewportsPanel->SetSingleViewIndex(RequestedSingleViewIndex);
		ViewportsPanel->SetLayoutMode(RequestedLayout);

		FEditorSettings& Settings = SettingsPanel->GetMutableSettings();
		Settings.bMultipleViewportsSingle = RequestedLayout == ELayoutMode::Single;
		Settings.MultipleViewportsSingleViewIndex = RequestedSingleViewIndex;
	}

	int32 PresetViewIndex = InvalidViewIndex;
	EMultipleViewportsCameraPreset RequestedPreset = EMultipleViewportsCameraPreset::Perspective;
	if (ViewportsPanel->ConsumeCameraPresetRequest(PresetViewIndex, RequestedPreset))
		ViewportsPanel->ApplyCameraPreset(PresetViewIndex, RequestedPreset);
	ViewportsPanel->UpdateLayout(ViewportSize);

	const float HorizontalDrag = ViewportsPanel->ConsumeHorizontalDrag();
	const float VerticalDrag = ViewportsPanel->ConsumeVerticalDrag();
	if (HorizontalDrag != 0.0f)
		ViewportsPanel->ApplySplitterDrag(EDragAxis::Horizontal, HorizontalDrag, ViewportSize);
	if (VerticalDrag != 0.0f)
		ViewportsPanel->ApplySplitterDrag(EDragAxis::Vertical, VerticalDrag, ViewportSize);
	if (HorizontalDrag != 0.0f || VerticalDrag != 0.0f)
	{
		ViewportsPanel->UpdateLayout(ViewportSize);
		const FSplitRatio Ratio = ViewportsPanel->GetSplitRatio();
		SettingsPanel->GetMutableSettings().MultipleViewportsHorizontal = Ratio.Horizontal;
		SettingsPanel->GetMutableSettings().MultipleViewportsVertical = Ratio.Vertical;
	}

	// 에셋 에디터 탭(파티클·스켈레탈 뷰어)의 뷰포트를 조작 중이면 레벨 View 입력을 받지 않는다.
	if (EditorUI->IsAnyTabHoveredOrInteracting())
		return;

	ViewportsPanel->UpdateInput(
		DeltaTime,
		LocalMousePosition,
		SettingsPanel->GetSettings().CameraSpeed,
		SettingsPanel->GetSettings().MouseSensitivity);
	// 겹친 창은 Hover 선택에서 제외하고 우클릭 Capture를 우선한다.
	if (ViewportsPanel->IsHovered() || ViewportsPanel->GetCapturedViewIndex() != InvalidViewIndex)
		ViewportsPanel->SetEditorViewIndex(ViewportsPanel->GetActiveViewIndex());
}

// 월드를 한 번 Tick한 뒤 에디터와 피킹을 갱신한다.
void UEditorEngine::TickWorldAndEditor(const float DeltaTime)
{
	// 월드 상태는 프레임마다 정확히 한 번 갱신한다.

	{
		SCOPE_CYCLE_COUNTER(STAT_WorldTick);
		if (bStepRequested && bIsPaused)
		{
			GetActiveWorld()->Tick(1.0f / 60.0f);
			bStepRequested = false;
		}
		else if(!bIsPaused)
			GetActiveWorld()->Tick(DeltaTime);

	}
	{
		SCOPE_CYCLE_COUNTER(STAT_EditorTick);
		EditorUI->Tick(DeltaTime);
	}
	// 렌더 준비는 View마다 그릴 월드가 다르므로 RenderAllViews에서 한다.
	// PIE 중에도 PIE가 아닌 View에서는 EditorWorld 편집을 계속한다.
	UpdateGizmoAndPicking();
	//SCOPE_CYCLE_COUNTER(STAT_UpdateAllTransforms);
	EditorWorld->GetScene().UpdateAllTransforms();
	if (PlayWorld && bIsPaused)
		PlayWorld->GetScene().UpdateAllTransforms();
}

// 모든 화면을 한 경로로 그린다. (Week11과 같은 구조)
// 보이는 탭이 CollectViews로 화면을 내놓고(레벨 탭은 레벨 View 넷, 에셋 에디터 탭은 프리뷰),
// 별도 PIE 창의 View를 끝에 붙인다. 그 뒤 활성 World를 준비하고 그림자를 만든 다음 View마다 렌더한다.
void UEditorEngine::RenderAllViews()
{
	// PIE 창 크기를 외부 View에 먼저 알려야 카메라 Override의 직교 폭 보정이 맞는다.
	bPIEWindowRendered = false;
	const bool bRenderPIEWindow = IsPIEInWindow() && UpdatePIEWindowTargets();

	// 월드 Tick으로 움직인 게임 카메라를 같은 프레임에 반영한다.
	UpdatePIEViewCamera();

	Renderer->SetLightDebugMode(SettingsPanel->GetSettings().LightDebugMode);
	Renderer->SetLightCullingMode(static_cast<ELightCullingMode>(SettingsPanel->GetSettings().LightCullingMode));

	// 1. 그릴 화면 수집. 가려진 탭은 건너뛴다.
	TArray<FEditorTabView> Views;
	EditorUI->CollectTabViews(Views);
	// AnimGraph 에디터는 탭이 아닌 기존 패널이라 문서별 미리보기 View를 따로 모은다.
	AnimGraphEditorPanel->CollectPreviewViews(Views);
	for (FEditorTabView& TabView : Views)
	{
		// 레벨 View는 자기 ID를 이미 가진다. 프리뷰처럼 비어 있으면 공용 프리뷰 ID를 준다.
		if (TabView.View.ShadowViewId == 0)
			TabView.View.ShadowViewId = PreviewShadowViewId;
	}
	int32 PIEWindowViewIndex = INDEX_NONE;
	if (bRenderPIEWindow && IsLevelViewRenderable(ExternalPIEViewport))
	{
		PIEWindowViewIndex = Views.Num();
		Views.Add(MakeLevelTabView(ExternalPIEViewport));
	}

	// 2. World 준비와 그림자 대상 선별
	// TODO : FSceneRenderer 도입전 임시. World 갱신 여부와 그림자 필요 여부를 별도로 관리한다.
	TArray<UWorld*> PreparedWorlds;
	TMap<UWorld*, TArray<const FEditorTabView*>> ShadowViews;
	const FEngineShowFlags& ShowFlags = SettingsPanel->GetSettings().ShowFlags;
	const bool bDrawPrimitives = ShowFlags.IsEnabled(EEngineShowFlag::Primitives);
	const bool bDynamicShadows = ShowFlags.IsEnabled(EEngineShowFlag::DynamicShadows);
	const float MinScreenRadius = std::max(0.0f, GMinScreenRadiusForShadowCaster);
	const float MinScreenRadiusSquared = MinScreenRadius * MinScreenRadius;

	for (const FEditorTabView& TabView : Views)
	{
		if (!TabView.World || !TabView.View.Client || !TabView.RenderingInfo)
			continue;

		// Transform·BVH 갱신과 동적 데이터 준비는 활성 World마다 한 번 수행한다.
		if (PreparedWorlds.Find(TabView.World) == INDEX_NONE)
		{
			SCOPE_CYCLE_COUNTER(STAT_PrepareRenderViews);
			TabView.World->BeginRenderViews();
			PreparedWorlds.Add(TabView.World);
		}

		// 이번 그림자 수신 연결 범위는 Lambert·Blinn-Phong이다.
		// 앞 View가 Unlit이어도 뒤의 Lit View에서 같은 World를 등록할 수 있다.
		const EViewMode ViewMode = TabView.View.Client->GetViewMode();
		const bool bNeedsShadow = bDrawPrimitives && bDynamicShadows
			&& (ViewMode == EViewMode::Lit_Lambert || ViewMode == EViewMode::Lit_BlinnPhong);
		if (bNeedsShadow)
			ShadowViews[TabView.World].Add(&TabView);
	}

	// 3. 그림자 작업 생성
	// TODO: FSceneRenderer 분리 시 World별 View 그룹 구성과 패스 조정을 이동한다.
	TArray<FProjectedShadowInfo> ShadowInfos;

	// World 공유 작업과 View별 Directional 작업을 같은 프레임 목록에 모은다.
	for (UWorld* World : PreparedWorlds)
	{
		const TArray<const FEditorTabView*>* WorldViews = ShadowViews.Find(World);
		if (!WorldViews)
			continue;
		SCOPE_CYCLE_COUNTER(ShadowStats::STAT_SetupCPU);
		// Directional에서도 같은 Context를 사용하도록 World 처리 동안 유지한다.
		TArray<FLODViewContext> ActiveShadowViews;
		{
			// 같은 World의 여러 View가 공유하는 지역광 준비 비용을 모은다.
			SCOPE_CYCLE_COUNTER(ShadowStats::SetupCPUStats[ShadowStats::Local]);
			ActiveShadowViews.Reserve(WorldViews->Num());
			TArray<FFrustumPlanes> ActiveViewFrustums;
			ActiveViewFrustums.Reserve(WorldViews->Num());

			for (const FEditorTabView* ShadowTabView : *WorldViews)
			{
				const FLODViewContext View = FEditorViewportClient::CalcLODViewContext(
					ShadowTabView->View.Client->GetEffectiveCamera(), ShadowTabView->View.Rect);
				ActiveShadowViews.Add(View);
				ActiveViewFrustums.Add(ExtractFrustumPlanes(View.ViewProjection));
			}

			World->GatherLocalProjectedShadows(ShadowInfos, ActiveViewFrustums, ActiveShadowViews, MinScreenRadiusSquared);
		}

		const FDirectionalLightSceneProxy* Directional = World->GetScene().GetPrimaryDirectionalLight();
		if (!Directional || !Directional->ShouldCastShadow())
			continue;

		const auto* Light = static_cast<const UDirectionalLightComponent*>(Directional->GetLightComponent());
		const float ShadowDistance = Light->GetShadowDistance();

		// 각 작업을 설정한 뒤 재사용하는 임시 계산 버퍼다.
		TArray<float> SplitDistances;
		TArray<FVector> ReceiverCorners;
		ReceiverCorners.Reserve(8);

		for (int32 ViewOffset = 0; ViewOffset < WorldViews->Num(); ++ViewOffset)
		{
			const FEditorSceneView& SceneView = (*WorldViews)[ViewOffset]->View;
			const FViewCamera& Camera = SceneView.Client->GetEffectiveCamera();
			const FLODViewContext& ShadowView = ActiveShadowViews[ViewOffset];

			// 0은 공유 작업에 남겨 두고, View가 가진 안정적인 ID를 쓴다.
			const uint32 ViewId = SceneView.ShadowViewId;
			const bool bOrthographic = ShadowView.bOrthographic;
			const uint32 ExpectedCascadeCount = !bOrthographic && Light->IsCSMEnabled()
				? static_cast<uint32>(Light->GetCascadeCount()) : 1u;
			const auto Group = ShadowStats::GetGroup(true, ExpectedCascadeCount);
			FScopeCycleCounter SetupCounter(ShadowStats::SetupCPUStats[Group]);
			ReceiverCorners.Reset();

			if (bOrthographic)
			{
				// 직교 View는 기존 논리 원점 앞뒤의 단일 범위를 유지한다.
				if (FEditorViewportClient::AppendShadowFrustumCorners(Camera, SceneView.Rect, ShadowDistance, ReceiverCorners))
				{
					World->GatherDirectionalShadow(ShadowInfos, ReceiverCorners, ShadowView,
						-ShadowDistance, ShadowDistance, MinScreenRadiusSquared, 0, ViewId);
				}
				continue;
			}

			// 패널 PIE와 별도 PIE 모두 실제 게임 카메라의 클립 범위를 사용한다.
			if (!Directional->CalculateCascadeSplitDistances(Camera.Projection.NearClip, Camera.Projection.FarClip, SplitDistances))
				continue;

			// 빈 큐가 생겨도 논리 Cascade 개수와 번호는 압축하지 않는다.
			const uint32 LogicalCascadeCount = static_cast<uint32>(SplitDistances.Num() - 1);
			for (int32 CascadeIndex = 0; CascadeIndex + 1 < SplitDistances.Num(); ++CascadeIndex)
			{
				float RenderNear = SplitDistances[CascadeIndex];
				if (CascadeIndex > 0)
				{
					const float PreviousLength = SplitDistances[CascadeIndex] - SplitDistances[CascadeIndex - 1];
					RenderNear -= PreviousLength * Light->GetCascadeTransitionFraction();
				}

				ReceiverCorners.Reset();
				if (!FEditorViewportClient::AppendShadowFrustumCorners(Camera, SceneView.Rect, RenderNear, SplitDistances[CascadeIndex + 1], ReceiverCorners))
					continue;

				// 각 Cascade는 자신이 속한 View의 크기 기준으로만 선별한다.
				World->GatherDirectionalShadow(ShadowInfos, ReceiverCorners, ShadowView,
					RenderNear, SplitDistances[CascadeIndex + 1], MinScreenRadiusSquared,
					static_cast<uint32>(CascadeIndex), ViewId, LogicalCascadeCount);
			}
		}
	}

	// 모든 World·View의 작업을 모은 뒤 Atlas 배정과 Depth 실행을 한 번 수행한다.
	// 빈 목록도 전달해 이전 프레임의 작업·바인딩·타일 할당을 정리한다.
	Renderer->RenderShadowMaps(std::move(ShadowInfos), *ShadowViewRenderer);

	// 4. View마다 렌더. 레벨 View·프리뷰·별도 PIE 창이 같은 경로를 쓴다.
	UWorld* PreparedWorld = nullptr;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FEditorTabView& TabView = Views[ViewIndex];
		if (!TabView.World || !TabView.View.Client || !TabView.RenderingInfo)
			continue;
		UWorld& ViewWorld = *TabView.World;

		if (&ViewWorld != PreparedWorld)
		{
			SCOPE_CYCLE_COUNTER(STAT_PrepareRenderViews);
			ViewWorld.GetScene().UpdateLights(Renderer);
			PreparedWorld = &ViewWorld;
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_BuildRenderQueue);
			FViewport::BuildRenderQueue(ViewWorld, TabView.View.Client->GetEffectiveCamera(), TabView.View.Rect,
				RenderQueue, IconQueue, ArrowQueue);
		}

		RenderFrame(TabView.View, ViewWorld, *TabView.RenderingInfo, RenderQueue, IconQueue, ArrowQueue);

		// 별도 PIE 창은 오프스크린 결과를 창 백버퍼로 옮긴다.
		if (ViewIndex == PIEWindowViewIndex)
			CopyPIEWindowToBackbuffer();
	}
}

FEditorTabView UEditorEngine::MakeLevelTabView(const FViewport& Viewport) const
{
	FEditorTabView TabView;
	TabView.World = GetWorldForView(&Viewport);
	TabView.View = MakeLevelSceneView(Viewport);
	TabView.RenderingInfo = Viewport.GetRenderingInfo();
	return TabView;
}

void UEditorEngine::CollectLevelViews(TArray<FEditorTabView>& OutViews) const
{
	for (int32 ViewIndex = 0; ViewIndex < FViewportsPanel::ViewCount; ++ViewIndex)
	{
		const FViewport& Viewport = ViewportsPanel->GetViewport(ViewIndex);
		if (IsLevelViewRenderable(Viewport))
			OutViews.Add(MakeLevelTabView(Viewport));
	}
}

bool UEditorEngine::IsLevelViewRenderable(const FViewport& Viewport) const
{
	// 별도 PIE 창 View는 창 크기와 게임 카메라가 모두 있을 때만 그린다.
	if (&Viewport == &ExternalPIEViewport)
		return Viewport.IsActive() && Viewport.GetClient().HasCameraOverride() && Viewport.GetRenderingInfo();
	return Viewport.IsActive() && Viewport.GetRenderingInfo();
}

FViewport* UEditorEngine::FindLevelViewportByShadowId(const uint32 ShadowViewId)
{
	if (ShadowViewId == ExternalPIEViewport.GetShadowViewId())
		return &ExternalPIEViewport;
	return ViewportsPanel ? ViewportsPanel->FindViewportByShadowId(ShadowViewId) : nullptr;
}

void UEditorEngine::UpdatePIEViewCamera() {
	if (!PlayWorld || !PIEViewport) {
		return;
	}

	APlayerCameraManager* CameraManager{ PlayWorld->GetPlayerCameraManager() };
	UCameraComponent* GameCamera{ CameraManager ? CameraManager->GetViewTargetCamera() : nullptr };
	if (!GameCamera || !CameraManager->HasValidCameraView()) {
		PIEViewport->GetClient().ClearCameraOverride();
		return;
	}

	// 게임 카메라 자체 입력(WASD·우클릭 회전·휠)은 PIE View를 선택했을 때만 받는다.
	// 별도 창이면 그 창이 OS 포커스를 가졌을 때다.
	const bool PIEViewFocused{ IsPIEInWindow() ? ::GetForegroundWindow() == PIEWindow->GetHandle() : ViewportsPanel->GetActiveViewport() == PIEViewport && !ImGui::GetIO().WantTextInput };
	GameCamera->SetExternalInputManaged(!PIEViewFocused);
	const FRect& Rect{ PIEViewport->GetRect() };
	if (Rect.Width > 0.0f && Rect.Height > 0.0f) {
		GameCamera->SetAspectRatio(Rect.Width / Rect.Height);
	}

	// 엔진 View 행렬에서 깊이축(View X)을 꺼내면 회전 규약과 무관하게 실제 렌더 방향과 같다.
	FMinimalViewInfo CameraView{ CameraManager->GetCameraView() };
	if (Rect.Width > 0.0f && Rect.Height > 0.0f) {
		CameraView.SetAspectRatio(Rect.Width / Rect.Height);
	}
	const FMatrix View{ CameraView.GetViewMatrix() };
	const FVector Forward{ View.M[0][0], View.M[1][0], View.M[2][0] };
	const FQuat Rotation{ CameraView.GetRotation().Quaternion() };

	const FCameraProjection Projection{ CameraView.GetIsOrthogonal() ? EProjectionMode::Orthographic : EProjectionMode::Perspective, CameraView.GetFieldOfView(), CameraView.GetOrthoWidth(), CameraView.GetNearZ(), CameraView.GetFarZ() };
	PIEViewport->GetClient().SetCameraOverride(CameraView.GetLocation(), Forward, Projection, Rect, &Rotation);
}

// 화면을 표시하고 UI 변경 후 View 설정을 보관한다.
void UEditorEngine::EndFrame()
{
	PresentFrame();
	// 메인 Swapchain은 FEngineLoop가 VSync 설정대로 Present하므로 PIE 창은 기다리지 않고 올린다.
	if (bPIEWindowRendered && PIESwapchain)
		PIESwapchain->SwapBuffers(0);
	// UI 변경 후 설정을 복사해 종료 시 카메라 수명에 의존하지 않는다.
	SettingsPanel->CaptureViewportSettings();
	// 프로파일러 반영
	FStatRegistry::EndFrame();
}

FWorldContext& UEditorEngine::GetEditorWorldContext()
{
	for (int32 i = 0; i < WorldList.Num(); ++i)
	{
		if (WorldList[i].WorldType == EWorldType::Editor)
		{
			return WorldList[i];
		}
	}

	return CreateNewWorldContext(EWorldType::Editor);
}

FWorldContext* UEditorEngine::GetPIEWorldContext(int32 WorldPIEInstance)
{
	for (FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.WorldType == EWorldType::PIE)
		{
			return &WorldContext;
		}
	}

	return nullptr;
}

// 입력 View의 Ray와 피킹으로 Gizmo·공유 선택을 갱신한다.
void UEditorEngine::RenderOpaquePass(const FEditorSceneView& View, const FRenderingInfo& ViewRenderingInfo, const FViewConstants& ViewConstants, 
	FRenderQueue& RenderQueue, UWorld& ViewWorld)
{
	const FMatrix& ViewProjection = ViewConstants.ViewProjection;
	const FVector& ViewCameraLocation = ViewConstants.CameraWorldPos;

	FRenderingInfo HDRInfo = ViewRenderingInfo;
	HDRInfo.ColorRenderTargets[0].Texture = View.HDRTarget;
	RenderCommand::BeginRenderPass(HDRInfo);

	const bool bDrawPrimitives = SettingsPanel->GetSettings().ShowFlags.IsEnabled(EEngineShowFlag::Primitives);
	// 삼각형 연결은 유지하고 View별 Fill Mode만 선택한다.
	const ERasterizerState SceneRasterizerState = View.Client->IsWireframe()
		? ERasterizerState::Wireframe : ERasterizerState::SolidBack;
	// View별 조명 뷰모드. 불투명·반투명 모두 이 값으로 UberLit 변형을 고른다.
	Renderer->SetViewMode(View.Client->GetViewMode());

	// 메시 파티클 콜백
	const FRenderer::FMeshParticlePacketDraw DrawMeshParticle =
	    [this](
	        const FRenderPacket& Packet,
	        const FMatrix& ViewProjection,
	        bool bDepthOnly)
	{
		if (!ParticleMeshRenderer)
			return;

		if (!ParticleMeshRenderer->Draw(
		        Packet,
		        Renderer->GetViewMode(),
		        bDepthOnly))
		{
			HTR_LOG(Error, "Failed to draw mesh particle batch.");
		}
	};

	// 렌더 루프 — 반드시 RenderAll보다 먼저
	// 직교는 시선이 모두 평행해 파노라마의 한 점만 확대되므로 하늘을 그리지 않고 지우기 색을 배경으로 쓴다.
	if (View.bDrawSkybox && !View.IsOrthographic())
		SkyboxRenderer->OnRender(ViewProjection, ViewCameraLocation);
	if (bDrawPrimitives)
	{
		RenderCommand::SetRasterizerState(SceneRasterizerState);
		RenderCommand::SetBlendState(EBlendState::Opaque);
		RenderCommand::SetDepthStencilState(EDepthStencilState::Default);

		RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		// 반투명은 Grid 뒤에 합성되어야 하므로 불투명만 먼저 그린다.
		Renderer->RenderQueueSorting(RenderQueue, ViewConstants);

		// 2.5D는 타일의 실제 깊이가 필요하므로 불투명 깊이를 먼저 채운 뒤 컬링한다.
		// 데칼도 PreDepth를 사용한다
		FTexture2D* DepthTexture = ViewRenderingInfo.DepthStencil.Texture;
		const bool bDepthPrepass = DepthTexture;
		if (bDepthPrepass)
		{
			Renderer->RenderDepth(ViewProjection, DrawMeshParticle);
			// DSV로 묶인 텍스처는 SRV로 읽을 수 없으므로 렌더 타깃을 푼다
			RenderCommand::EndRenderPass(HDRInfo);
		}

		Renderer->CullLights(
			View.CalcView(),
			View.CalcProjection(),
			ViewRenderingInfo.ViewportSetting.Width,
			ViewRenderingInfo.ViewportSetting.Height,
			bDepthPrepass ? DepthTexture : nullptr);
		
		if (bDepthPrepass)
		{
			DecalRenderer->OnRender(
				Renderer,
				ViewWorld.GetScene().DecalSceneProxies,
				DepthTexture,
				ViewProjection,
				ViewConstants.CameraWorldPos,
				View.CalcProjection(),
				ViewRenderingInfo.ViewportSetting,
				SettingsPanel->GetSettings().ShowFlags.IsEnabled(EEngineShowFlag::Decals));

			RenderCommand::SetRasterizerState(SceneRasterizerState);

			// 스카이박스와 Prepass 깊이를 지우지 않도록 Load로 다시 묶는다
			FRenderingInfo ResumeInfo = HDRInfo;
			for (FRenderingDesc& Color : ResumeInfo.ColorRenderTargets)
				Color.LoadOp = ERenderTargetLoadOp::Load;
			ResumeInfo.DepthStencil.LoadOp = ERenderTargetLoadOp::Load;
			RenderCommand::BeginRenderPass(ResumeInfo);
		}

		// 스켈레탈 프리뷰의 본 웨이트 히트맵: 정점별 웨이트를 VS t32에 묶는다. 앞의 깊이·데칼·라이트 컬링 패스가 슬롯을 덮어쓸 수 있어 그리기 직전에 묶는다.
		if (View.BoneWeightHeatmap && View.BoneWeightHeatmapStatus == FBoneWeightHeatmap::StatusOn)
			View.BoneWeightHeatmap->Bind();
		Renderer->RenderOpaque(ViewProjection, DrawMeshParticle);
		// 장면 Wireframe이 Grid·Gizmo·UI로 전파되지 않도록 복원한다.
		RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
	}



	if (bDrawPrimitives)
	{
		// Grid 파이프라인이 바꾼 상태를 장면 기준으로 되돌린 뒤 반투명을 먼 것부터 그린다.
		RenderCommand::SetRasterizerState(SceneRasterizerState);
		RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// 실제 뷰 카메라를 사용한다.
		// PIE 카메라 Override도 포함한다.
		const auto& Camera =
			View.Client->GetEffectiveCamera();

		FParticleSpriteView ParticleView{};

		ParticleView.ViewProjection = ViewProjection;
		ParticleView.CameraPosition = Camera.Transform.Location;

		ParticleView.CameraRight =
			Camera.Transform.Rotation.GetRightVector();

		ParticleView.CameraUp =
			Camera.Transform.Rotation.GetUpVector();
			
		ParticleView.CameraForward =
			Camera.Transform.Rotation.GetForwardVector();
	
		ParticleView.bOrthographic =
			Camera.Projection.Mode != EProjectionMode::Perspective;

		Renderer->RenderTranslucent(
			ViewProjection,
		    [this, &ParticleView](const FRenderPacket& Packet) // DrawSpriteParticle
			{
			    if (!ParticleSpriteRenderer)
				    return;

				const bool bSuccess =
			        ParticleSpriteRenderer->Draw(
			            Packet.SpriteInstances,
			            Packet.Material,
			            ParticleView,
			            Packet.ParticleScreenAlignment,
			            Packet.ParticleAxisLock);

				if (!bSuccess)
				{
				    HTR_LOG(
				        Error,
				        "Failed to draw sprite particle batch.");
				}
			},
			DrawMeshParticle,
			[this, &ParticleView](const FRenderPacket& Packet) // DrawStripParticle
			{
			    if (!ParticleStripRenderer)
				    return;

				if (!ParticleStripRenderer->Draw(
					Packet.StripPoints,
					Packet.Material,
					ParticleView))
				{
					HTR_LOG(
				        Error,
				        "Failed to draw strip particle batch.");
				}
			});

		RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
		RenderCommand::SetBlendState(EBlendState::Opaque);
		RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
	}



	RenderCommand::EndRenderPass(HDRInfo);
}

void UEditorEngine::RenderFogPass(const FEditorSceneView& View, UWorld& ViewWorld, const FRenderingInfo& ViewRenderingInfo, const FVector& ViewCameraLocation, const FMatrix& ViewProjection, FRenderQueue& RenderQueue)
{
	FRenderingInfo HDRInfo = ViewRenderingInfo;
	HDRInfo.ColorRenderTargets[0].Texture = View.HDRTarget;
	const FFogInfo* FogInfo{ ViewWorld.GetScene().GetFogInfo() };

	if (!FogInfo) {
		return;
	}

	FRenderingInfo PassInfo = HDRInfo;

	//Load는 기존 내용을 그대로 두고 그 위에 그림을 그린다는뜻
	for (FRenderingDesc& Color : PassInfo.ColorRenderTargets)
		Color.LoadOp = ERenderTargetLoadOp::Load;

	PassInfo.DepthStencil.Texture = nullptr;
	RenderCommand::BeginRenderPass(PassInfo);

	FogRenderer->OnRender(HDRInfo.DepthStencil.Texture, ViewProjection, ViewCameraLocation, *FogInfo);

	//Fog는 여러개있더라도 1개의 Fog만 인식해서 그리도록 해야한다.

	RenderCommand::EndRenderPass(PassInfo);
}

void UEditorEngine::RenderOverlayPass(const FEditorSceneView& View, UWorld& ViewWorld, const FRenderingInfo& ViewRenderingInfo, const FMatrix& ViewProjection, const FVector& ViewCameraLocation, const FVector& ViewCameraForward, FRenderQueue& RenderQueue, FRenderQueue& IconQueue, FRenderQueue& ArrowQueue)
{
	// 실행 중인 PIE View에는 Grid·Gizmo·Outline 같은 에디터 표시를 그리지 않는다.
	// 별도 PIE 창은 편집 입력이 없으므로 일시정지 중에도 게임 화면만 보인다.
	const bool bShowEditorOverlay = View.bShowEditorOverlay;
	const FEngineShowFlags& ShowFlags = SettingsPanel->GetSettings().ShowFlags;
	FRenderingInfo PassInfo = ViewRenderingInfo;

	for (FRenderingDesc& Color : PassInfo.ColorRenderTargets)
		Color.LoadOp = ERenderTargetLoadOp::Load;

	PassInfo.DepthStencil.LoadOp = ERenderTargetLoadOp::Load;


	RenderCommand::BeginRenderPass(PassInfo);

	if (bShowEditorOverlay && !ShowFlags.IsEnabled(EEngineShowFlag::DepthView))
	{
		// 라인 배처는 매 프레임 한 번만 비우고 한 번만 그린다.
		// 바운딩박스는 그 안에 쌓이는 여러 항목 중 하나일 뿐이다.
		LineBatcher->BeginFrame();

		if (ShowFlags.IsEnabled(EEngineShowFlag::Bounds))
		{
			LineBatcher->BuildVertexBuffer(ViewWorld);
		}

		// 선택된 액터가 라이트면 원뿔을 같이 쌓는다
		if (Gizmo->GetTarget() && Gizmo->GetTarget()->GetOwner() && Gizmo->GetTarget()->GetOwner()->GetWorld() == &ViewWorld)
		{
			if (ALightActorBase* LightActor = Cast<ALightActorBase>(Gizmo->GetTarget()->GetOwner()))
			{
				LightActor->GetLightComponent()->DrawDebug(LineBatcher.get());
			}
			else if (ADecalActor* DecalActor = Cast<ADecalActor>(Gizmo->GetTarget()->GetOwner()))
			{
				DecalActor->GetDecalComponent()->DrawDebug(LineBatcher.get());
			}
		}

		TArray<UShapeComponent*> Shapes;
		for (auto Iter = TObjectIterator<UShapeComponent>(); Iter; Iter++)
		{
			if (Iter->GetWorld() == &ViewWorld)
				Shapes.Add(*Iter);
		}

		const AActor* SelectedActor = Gizmo->GetTarget() ? Gizmo->GetTarget()->GetOwner() : nullptr;
		for (UShapeComponent* Shape : Shapes)
		{
			if (Shape->ShouldDrawOnlyIfSelected() && Shape->GetOwner() != SelectedActor)
				continue;

			bool bOverlapping = false;
			for (UShapeComponent* Other : Shapes)
			{
				if (Shape->IsOverlapping(*Other))
				{
					bOverlapping = true;
					break;
				}
			}

			Shape->DrawDebug(*LineBatcher, bOverlapping);
		}

		LineBatcher->OnRender(ViewProjection);
	}

	//그리드
	//TODO : 그리드도 안개가 적용되어야하나..?
	// 언리얼처럼 투영에 따라 방식을 고정한다. 원근은 PS 그리드, 직교는 보는 평면에 맞춘 배치 라인 그리드다.
	const bool bDrawGrid = View.GridMode == EViewGridMode::Show
		|| (View.GridMode == EViewGridMode::FollowOverlay && bShowEditorOverlay && SettingsPanel->GetSettings().mShowGrid);
	if (bDrawGrid)
	{
		const EGridPlane GridPlane = View.Client->GetGridPlane();

		if (!View.IsOrthographic())
		{
			GridRenderer->OnRenderPSGrid(ViewProjection, ViewCameraLocation, SettingsPanel->GetSettings(), PassInfo.ViewportSetting);
		}
		else
		{
			GridRenderer->OnRenderBatchGrid(
				ViewProjection,
				ViewCameraLocation,
				ViewCameraForward,
				GridPlane,
				static_cast<float>(SettingsPanel->GetSettings().GridSpacing),
				// 이 분기는 항상 직교다. 자유 직교 View만 세 축을 모두 그린다.
				View.Client->GetCameraPreset() == EMultipleViewportsCameraPreset::OrthographicView,
				PassInfo.ViewportSetting
			);
		}
	}

	// 스켈레탈 프리뷰의 본과 본 Gizmo. (Week11처럼 프리뷰는 무엇을 그릴지만 View에 싣고, 그리기는 여기서 한다)
	// 둘 다 장면 깊이를 지우고 그려서 메시에 가려지지 않는다.
	if (View.Skeleton && SkeletonRenderer)
		SkeletonRenderer->Render(*View.Skeleton, ViewProjection, PassInfo.DepthStencil.Texture);
	if (View.PreviewGizmo && View.PreviewGizmo->GetTarget())
	{
		RenderCommand::ClearDepthStencil(PassInfo.DepthStencil.Texture);
		RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
		GizmoRenderer->OnRender(*View.PreviewGizmo, ViewProjection, ViewCameraLocation, View.IsOrthographic());
	}

	// Editor Icon (안개, Light Actor) + 빛 방향 Arrow
	// TODO: 모든 프리미티브를 공통 큐로 통합하고 패스별로 렌더하도록 별도 IconQueue·ArrowQueue를 제거한다.
	if (bShowEditorOverlay)
	{
		RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
		RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// 아이콘 먼저 그리기 (반투명)
		Renderer->RenderQueueSorting(IconQueue, ViewProjection, ViewCameraLocation, ViewCameraForward);
		Renderer->RenderTranslucent(ViewProjection);

		// 화살표 그리기 (불투명)
		Renderer->RenderQueueSorting(ArrowQueue, ViewProjection, ViewCameraLocation, ViewCameraForward);
		Renderer->RenderOpaque(ViewProjection);
		Renderer->RenderTranslucent(ViewProjection);	// 패킷 뒷정리
	}

	// TextRenderComponent 렌더링
	for (TObjectIterator<UTextRenderComponent> TextComponent; TextComponent; ++TextComponent)
	{
		if (!TextComponent || !TextComponent->GetFont() || !TextComponent->IsVisible())
		{
			continue;
		}
		// 에디터·PIE 월드의 컴포넌트가 함께 순회되므로 이 View의 월드만 그린다.
		if (!TextComponent->GetOwner() || TextComponent->GetOwner()->GetWorld() != &ViewWorld)
		{
			continue;
		}

		TextRenderer->OnRender(
			TextComponent->GetText(),
			TextComponent->GetWorldMatrix(),
			TextComponent->GetTextSize(),
			*TextComponent->GetFont(),
			ViewProjection
		);
	}

	// 선택 대상은 한 월드에만 속하므로 그 월드를 그리는 View에만 표시한다.
	const auto IsInViewWorld = [&ViewWorld](const UActorComponent* Component)
	{
		return Component && Component->GetOwner() && Component->GetOwner()->GetWorld() == &ViewWorld;
	};

	// 스텐실 기반이라 선택 대상의 가시성이 꺼져 있어도 외곽선만 그린다.
	if (bShowEditorOverlay && IsInViewWorld(Outline->GetTarget()))
	{
		OutlineRenderer->OnRender(*Outline, ViewProjection, PassInfo.ViewportSetting);
	}

	if (bShowEditorOverlay && IsInViewWorld(Gizmo->GetTarget()))
	{
		//auto Target = Cast<UPrimitiveComponent>(Gizmo->GetTarget());
		//FBox box = Target->CalcBounds();

		RenderCommand::ClearDepthStencil(ViewRenderingInfo.DepthStencil.Texture);

		GizmoRenderer->OnRender(
			*Gizmo,
			ViewProjection,
			ViewCameraLocation,
			View.IsOrthographic());
	}

	RenderCommand::ClearDepthStencil(PassInfo.DepthStencil.Texture);

	if (ShowFlags.IsEnabled(EEngineShowFlag::UUID))
	{
		// 월드 +Z가 아니라 화면 위쪽으로 띄워야 Top·Bottom처럼 Z축을 따라 보는 View에서도 물체를 가리지 않는다.
		const FVector ScreenUp = FEditorViewportClient::NormalizedOrZero(View.Client->GetEffectiveCamera().Transform.Rotation.GetUpVector());

		for (AActor* Actor : ViewWorld.GetPersistentLevel()->GetActors())
		{
			if (!Actor)
				continue;

			FBox ActorBounds;
			if (!Actor->TryGetActorBounds(ActorBounds))
			{
				const FVector ActorLocation = Actor->GetActorLocation();
				ActorBounds.Min = ActorLocation;
				ActorBounds.Max = ActorLocation;
			}

			const FVector Center = (ActorBounds.Min + ActorBounds.Max) * 0.5f;
			const FVector HalfExtent = (ActorBounds.Max - ActorBounds.Min) * 0.5f;
			// 바운드 박스가 화면 위쪽으로 뻗은 길이만큼 올린 뒤 여백을 더한다.
			const float ExtentAlongUp = std::fabs(ScreenUp.X) * HalfExtent.X
				+ std::fabs(ScreenUp.Y) * HalfExtent.Y
				+ std::fabs(ScreenUp.Z) * HalfExtent.Z;
			UUIDLocation = Center + ScreenUp * (ExtentAlongUp + 0.5f);

			FString Text =
				"UUID : " + std::to_string(Actor->GetUUID());

			TextRenderer->BuildTextMesh(
				Text,
				0.5f,
				*SystemFont
			);

			const FMatrix BillboardWorld = FEditorViewportClient::BuildBillboardMatrix(View.Client->GetEffectiveCamera(), UUIDLocation, 1.0f, 1.0f);
			TextRenderer->OnRender(Text, BillboardWorld, 0.5f, *SystemFont, ViewProjection);
		}
	}

	if (View.bRenderGameUI)
	{
		UIRenderer->RenderUIImage(ViewRenderingInfo.ViewportSetting.Width, ViewRenderingInfo.ViewportSetting.Height);
		UIRenderer->RenderUIText(ViewRenderingInfo.ViewportSetting.Width, ViewRenderingInfo.ViewportSetting.Height);
	}

	RenderCommand::EndRenderPass(PassInfo);
}

void UEditorEngine::RenderFXAAPass(const FEditorSceneView& View, const FRenderingInfo& ViewRenderingInfo)
{
	FTexture2D* SceneColor = ViewRenderingInfo.ColorRenderTargets[0].Texture;
	FTexture2D* FxaaTarget = View.FxaaTarget;   // 뷰별 임시 텍스처
	if (!SceneColor || !FxaaTarget)
		return;

	// 출력은 임시 텍스처, 깊이 없음, 어차피 전부 덮어쓰니 Clear 불필요
	FRenderingInfo PassInfo;
	PassInfo.ViewportSetting = ViewRenderingInfo.ViewportSetting;

	FRenderingDesc ColorDesc{};
	ColorDesc.Texture = FxaaTarget;
	ColorDesc.LoadOp = ERenderTargetLoadOp::DontCare;
	PassInfo.ColorRenderTargets.Add(ColorDesc);
	PassInfo.DepthStencil.Texture = nullptr;

	//장면 텍스처를 읽어서 임시 텍스처에 FXAA 결과를 그린다
	RenderCommand::BeginRenderPass(PassInfo);
	FXAARenderer->OnRender(SceneColor);
	RenderCommand::EndRenderPass(PassInfo);

	// SceneColor가 SRV로 남아 있으면 Overlay 패스에서 RTV로 쓸 때 D3D가 강제로 해제하므로 먼저 비운다
	RenderCommand::UnbindShaderResource(0, EShaderBindFlagBits::Pixel);

	RenderCommand::CopyTexture(SceneColor, FxaaTarget);
}

void UEditorEngine::RenderToneMapPass(const FEditorSceneView& View, UWorld& ViewWorld, const FRenderingInfo& ViewRenderingInfo)
{
	FTexture2D* LDRColor = ViewRenderingInfo.ColorRenderTargets[0].Texture;
	FTexture2D* HDRColor = View.HDRTarget;   // 뷰별 임시 텍스처
	if (!LDRColor || !HDRColor)
		return;

	// 출력은 임시 텍스처, 깊이 없음, 어차피 전부 덮어쓰니 Clear 불필요
	FRenderingInfo PassInfo;
	PassInfo.ViewportSetting = ViewRenderingInfo.ViewportSetting;

	FRenderingDesc ColorDesc{};
	ColorDesc.Texture = LDRColor;
	ColorDesc.LoadOp = ERenderTargetLoadOp::DontCare;
	PassInfo.ColorRenderTargets.Add(ColorDesc);
	PassInfo.DepthStencil.Texture = nullptr;

	// 뷰모드에 따라 톤 매핑 방식을 고른다.
	// - Lit: 노출 → ACES → sRGB 인코딩
	// - Unlit: 텍스처 원본 색이 보이도록 sRGB 인코딩만
	// - WorldNormal·Light Heatmap: 셰이더가 쓴 디버그 값을 그대로
	EToneMapMode ToneMapMode = EToneMapMode::ACES;
	const EViewMode ViewMode = View.Client->GetViewMode();
	if (ViewMode == EViewMode::WorldNormal || SettingsPanel->GetSettings().LightDebugMode == 1)
		ToneMapMode = EToneMapMode::Passthrough;
	else if (ViewMode == EViewMode::Unlit)
		ToneMapMode = EToneMapMode::EncodeOnly;

	//HDR 장면 텍스처를 읽어서 LDR 뷰 텍스처에 ToneMap 결과를 그린다
	RenderCommand::BeginRenderPass(PassInfo);
	// 애셋 프리뷰는 자기 노출 값을 쓴다. (Preview Scene Settings의 Exposure)
	ToneMapRenderer->SetExposure(View.ExposureOverride >= 0.0f ? View.ExposureOverride : SettingsPanel->GetSettings().Exposure);
	const FVignetteSettings& Vignette = ViewWorld.GetVignetteSettings();
	ToneMapRenderer->SetVignette(
		Vignette.GetEffectiveIntensity(),
		Vignette.Radius,
		Vignette.Softness,
		Vignette.Color);
	ToneMapRenderer->OnRender(HDRColor, ToneMapMode, ASequenceActor::GetWorldFadeAlpha(&ViewWorld));
	RenderCommand::EndRenderPass(PassInfo);

	// SceneColor가 SRV로 남아 있으면 Overlay 패스에서 RTV로 쓸 때 D3D가 강제로 해제하므로 먼저 비운다
	RenderCommand::UnbindShaderResource(0, EShaderBindFlagBits::Pixel);
}

void UEditorEngine::RenderDepthPass(const FEditorSceneView& View, const FRenderingInfo& ViewRenderingInfo, const FMatrix& ViewProjection, const FVector& ViewCameraLocation, const FVector& ViewCameraForward)
{
	FRenderingInfo PassInfo = ViewRenderingInfo;

	for (FRenderingDesc& Color : PassInfo.ColorRenderTargets)
		Color.LoadOp = ERenderTargetLoadOp::DontCare;
	PassInfo.DepthStencil.Texture = nullptr;

	RenderCommand::BeginRenderPass(PassInfo);
	DepthViewRenderer->OnRender(ViewRenderingInfo.DepthStencil.Texture, ViewProjection, ViewCameraLocation, ViewCameraForward);
	RenderCommand::EndRenderPass(PassInfo);
}

// 입력 View의 Ray와 피킹으로 Gizmo·공유 선택을 갱신한다.
void UEditorEngine::UpdateGizmoAndPicking()
{
	if (EditorUI->IsAnyTabHoveredOrInteracting())
		return;

	// Delete는 BeginFrame에서 한 번만 처리하고 여기서는 View 입력만 다룬다.
	FViewport* ActiveViewport = ViewportsPanel->GetActiveViewport();
	if (!ActiveViewport || !ViewportsPanel->IsHovered())
		return;
	// 실행 중인 PIE View는 편집하지 않는다. 일시정지 중이면 PlayWorld를 피킹한다.
	if (IsPlayingView(ActiveViewport))
		return;

	const FVector2 LocalMousePosition = ViewportsPanel->GetLocalMousePosition();
	FRay Ray{};
	if (!ViewportsPanel->TryGetActiveViewRay(LocalMousePosition, Ray))
		return;

	const FRect& Rect = ActiveViewport->GetRect();
	const FVector2 ViewLocalMouse = ActiveViewport->ToViewLocal(LocalMousePosition);
	const FMatrix ViewProjection = ActiveViewport->GetViewProjection();
	bool bMouseDown = FInputSystem::IsMouseDown(EMouseButton::Left);

	// 다른 월드의 선택 대상은 이 View에 Gizmo가 보이지 않으므로 조작하지 않는다.
	UWorld* ViewWorld = GetWorldForView(ActiveViewport);
	const USceneComponent* GizmoTarget = Gizmo->GetTarget();
	const bool bGizmoInView = !GizmoTarget ||
		(GizmoTarget->GetOwner() && GizmoTarget->GetOwner()->GetWorld() == ViewWorld);

	if (bGizmoInView)
	{
		Gizmo->Update(
			Ray,
			ViewLocalMouse,
			ViewProjection,
			static_cast<int>(Rect.Width),
			static_cast<int>(Rect.Height),
			bMouseDown,
			ActiveViewport->GetCameraLocation(),
			ActiveViewport->IsOrthographic());
	}

	const bool bGizmoBlocksPick = bGizmoInView && (Gizmo->IsUsing() || Gizmo->GetHoveredAxis() >= 0);
	if (FInputSystem::IsMousePressed(EMouseButton::Left) && !bGizmoBlocksPick)
	{
		// Billboard는 렌더링에 사용한 View와 동일한 행렬로 교차를 판정한다.
		const auto ResolveBillboardTransform = [](const UBillboardComponent& Billboard, const void* Context) -> FMatrix
		{
			const auto& Viewport = *static_cast<const FViewport*>(Context);
			const FVector2 Size = Billboard.GetRenderSize();
			return Viewport.BuildBillboardMatrix(Billboard.GetWorldLocation(), Size.X, Size.Y);
		};

		// Gizmo에 사용한 Ray를 재사용하고, 월드의 피킹 결과를 선택에 직접 반영한다.
		FHitResult Hit;
		const bool bHit = ViewWorld->LineTraceSingle(Ray, Hit, ResolveBillboardTransform, ActiveViewport);
		OutlinerPanel->SelectActor(bHit ? Hit.HitComponent->GetOwner() : nullptr);
		if (bHit)
			Outline->SetTarget(Hit.HitComponent);
	}
}

// View가 가진 상태와 타깃을 RenderFrame 입력 하나로 묶는다.
FEditorSceneView UEditorEngine::MakeLevelSceneView(const FViewport& Viewport) const
{
	FEditorSceneView View;
	View.Client = &Viewport.GetClient();
	View.Rect = Viewport.GetRect();
	View.HDRTarget = Viewport.GetHDRTarget();
	View.FxaaTarget = Viewport.GetFxaaTarget();
	// 0은 World 공유 그림자 작업에 남겨 둔다. RenderAllViews의 그림자 작업 생성과 같은 ID다.
	View.ShadowViewId = Viewport.GetShadowViewId();
	// 실행 중인 PIE View와 별도 PIE 창에는 Grid·Gizmo·Outline 같은 에디터 표시를 그리지 않는다.
	View.bShowEditorOverlay = !IsPlayingView(&Viewport) && &Viewport != &ExternalPIEViewport;
	View.bRenderGameUI = PlayWorld && &Viewport == PIEViewport;
	return View;
}

void UEditorEngine::OpenParticleSystemEditor(UParticleSystem* Asset)
{
	if (!Asset)
		return;

	// 같은 에셋을 편집 중인 탭이 있으면 그 탭을 띄운다. (탭 두 개가 한 에셋을 동시에 고치지 않게)
	TArray<int32> UsedSlots;
	for (const TUniquePtr<FEditorTab>& Tab : EditorUI->GetTabs())
	{
		auto* ParticleTab = dynamic_cast<FParticleSystemEditorTab*>(Tab.get());
		if (!ParticleTab)
			continue;
		if (ParticleTab->GetEditingAsset() == Asset)
		{
			ParticleTab->RequestFocus();
			return;
		}
		UsedSlots.Add(ParticleTab->GetSlot());
	}

	// 가장 작은 빈 번호를 쓴다. 닫았던 번호를 다시 쓰면 imgui.ini에 남은 그 탭의 패널 배치를 재사용한다.
	int32 Slot = 0;
	while (UsedSlots.Find(Slot) != INDEX_NONE)
		++Slot;

	if (!EditorUI->AddTab<FParticleSystemEditorTab>(Slot, Asset, [this](UParticleSystem* Requested) { OpenParticleSystemEditor(Requested); }))
		HTR_LOG(Error, "Failed to create particle system editor tab.");
}

void UEditorEngine::OpenSkeletalMeshViewer(USkeletalMesh* Mesh)
{
	// 같은 메시를 보고 있는 탭이 있으면 그 탭을 띄운다. 메시 없이 여는 빈 뷰어는 항상 새로 만든다.
	TArray<int32> UsedSlots;
	for (const TUniquePtr<FEditorTab>& Tab : EditorUI->GetTabs())
	{
		auto* SkeletalTab = dynamic_cast<FSkeletalMeshViewerTab*>(Tab.get());
		if (!SkeletalTab || SkeletalTab->IsSharedPreviewTab())
			continue;
		if (Mesh && SkeletalTab->GetEditingAsset() == Mesh)
		{
			SkeletalTab->RequestFocus();
			return;
		}
		UsedSlots.Add(SkeletalTab->GetSlot());
	}

	// 가장 작은 빈 번호를 쓴다. 닫았던 번호를 다시 쓰면 imgui.ini에 남은 그 탭의 패널 배치를 재사용한다.
	int32 Slot = 0;
	while (UsedSlots.Find(Slot) != INDEX_NONE)
		++Slot;

	FSkeletalMeshViewerTab* NewTab = EditorUI->AddTab<FSkeletalMeshViewerTab>(Slot, Mesh,
		[this](USkeletalMesh* Requested) { OpenSkeletalMeshViewer(Requested); });
	if (!NewTab)
	{
		HTR_LOG(Error, "Failed to create skeletal mesh viewer tab.");
		return;
	}
	NewTab->Animation.SetOpenBlendSpaceEditorCallback([this](const FString& Path) { BlendSpaceEditorPanel->OpenAsset(Path); });
}

void UEditorEngine::OpenSkeletalMeshFile(const FString& Path)
{
	const FFbxAssetSet* Assets = UAssetManager::Get().LoadFbxAssets(Path);
	if (!Assets)
	{
		HTR_LOG(Error, "Could not open FBX asset: {}", Path);
		return;
	}
	if (!Assets->SkeletalMesh)
	{
		HTR_LOG(Info, "Imported {} static mesh assets from {}. This FBX has no skeletal mesh to preview.",
			Assets->StaticMeshes.Num(), Path);
		return;
	}
	OpenSkeletalMeshViewer(Assets->SkeletalMesh);
}

void UEditorEngine::OpenParticleSystemFile(const FString& Path)
{
	// 경로 → 에셋 키 변환과 "이미 불러왔으면 그 객체를 돌려주기"는 LoadParticleSystem이 맡는다.
	FString Error;
	UParticleSystem* Asset = UAssetManager::Get().LoadParticleSystem(Path, Error);
	if (!Asset)
	{
		HTR_LOG(Error, "Failed to open particle system '{}': {}", Path, Error);
		return;
	}
	OpenParticleSystemEditor(Asset);
}

// View 행렬로 Scene·Grid·Gizmo·텍스트·Outline을 렌더한다.
void UEditorEngine::RenderFrame(const FEditorSceneView& View, UWorld& ViewWorld, const FRenderingInfo& ViewRenderingInfo,
	FRenderQueue& RenderQueue, FRenderQueue& IconQueue, FRenderQueue& ArrowQueue)
{
	// 카메라에서 나오는 값은 호출자가 따로 넘기지 않고 View 하나에서 계산한다.
	const FViewCamera& ViewCamera = View.Client->GetEffectiveCamera();
	const FMatrix ViewProjection = FEditorViewportClient::CalcViewProjection(ViewCamera, View.Rect);
	const FVector ViewCameraLocation = ViewCamera.Transform.Location;
	const FVector ViewCameraForward = FEditorViewportClient::NormalizedOrZero(ViewCamera.Transform.Rotation.GetForwardVector());

	if (!View.IsOrthographic() && &ViewWorld == GetActiveWorld() && !bIsPaused)
	{
		ViewWorld.AddViewLocationRendered(ViewCameraLocation);
	}

	const FEngineShowFlags& ShowFlags = SettingsPanel->GetSettings().ShowFlags;

	// GPU의 int4·float4와 Component의 분할 상한이 일치해야 한다.
	static_assert(UDirectionalLightComponent::MaxShadowCascades == 4);

	FViewConstants ViewConstants{};
	ViewConstants.ViewProjection = ViewProjection;
	ViewConstants.CameraWorldPos = ViewCameraLocation;
	ViewConstants.CameraForward = ViewCameraForward;
	// 패널과 별도 PIE 모두 해당 Client의 표시 상태를 전달한다.
	ViewConstants.VisualizeShadowCascades = View.Client->GetShowFlags().IsEnabled(EEngineShowFlag::ShadowCascades) ? 1u : 0u;
	// 스켈레탈 프리뷰의 본 웨이트 히트맵. 레벨 View는 0(꺼짐)이다.
	ViewConstants.BoneWeightHeatmapStatus = View.BoneWeightHeatmapStatus;


	// 작업 생성과 동일한 표시 조건으로 Directional 수신 데이터를 준비한다.
	const EViewMode ViewMode = View.Client->GetViewMode();
	const bool bUseDirectionalShadows =
		ShowFlags.IsEnabled(EEngineShowFlag::Primitives)
		&& ShowFlags.IsEnabled(EEngineShowFlag::DynamicShadows)
		&& (ViewMode == EViewMode::Lit_Lambert || ViewMode == EViewMode::Lit_BlinnPhong);

	const FDirectionalLightSceneProxy* Directional = bUseDirectionalShadows ? ViewWorld.GetScene().GetPrimaryDirectionalLight() : nullptr;

	if (Directional && Directional->ShouldCastShadow())
	{
		TArray<float> SplitDistances;

		if (View.IsOrthographic())
		{
			// 직교 작업 생성과 같은 논리 원점 앞뒤의 단일 구간이다.
			const auto* Light = static_cast<const UDirectionalLightComponent*>(Directional->GetLightComponent());
			const float ShadowDistance = Light->GetShadowDistance();
			SplitDistances.Add(-ShadowDistance);
			SplitDistances.Add(ShadowDistance);
		}
		else
		{
			// 작업 생성에 사용한 것과 같은 EffectiveCamera·분할 규칙을 사용한다.
			const FViewCamera& Camera =	View.Client->GetEffectiveCamera();
			Directional->CalculateCascadeSplitDistances(Camera.Projection.NearClip, Camera.Projection.FarClip, SplitDistances);
		}

		// 분할 실패 시 배열이 비어 있으므로 기본 CascadeCount 0을 유지한다.
		if (SplitDistances.Num() >= 2)
		{
			const uint32 ViewId = View.ShadowViewId;
			const uint32 CascadeCount = static_cast<uint32>(SplitDistances.Num() - 1);
			ViewConstants.DirectionalCascadeCount = CascadeCount;
			ViewConstants.DirectionalShadowNearDistance = SplitDistances[0];
			if (CascadeCount > 1)
			{
				const auto* Light = static_cast<const UDirectionalLightComponent*>(Directional->GetLightComponent());
				ViewConstants.DirectionalCascadeTransitionFraction = Light->GetCascadeTransitionFraction();
			}
			for (uint32 CascadeIndex = 0; CascadeIndex < CascadeCount; ++CascadeIndex)
			{
				// GPU 인덱스는 연속 번호라고 가정하지 않고 각각 조회한다.
				ViewConstants.DirectionalShadowIndices[CascadeIndex] = Renderer->GetShadowIndex(*Directional, CascadeIndex, ViewId);
				ViewConstants.DirectionalCascadeFarDistances[CascadeIndex] = SplitDistances[CascadeIndex + 1];
			}
		}
	}

	// 준비한 View 상수를 장면 패스에 전달한다.
	RenderOpaquePass(View, ViewRenderingInfo, ViewConstants, RenderQueue, ViewWorld);
	if (View.BoneWeightHeatmap)
		View.BoneWeightHeatmap->Unbind();

	if (ShowFlags.IsEnabled(EEngineShowFlag::DepthView))
	{
		RenderDepthPass(View, ViewRenderingInfo, ViewProjection, ViewCameraLocation, ViewCameraForward);

	}
	else
	{
		//직교인 경우 안개 Pass를 그리지 않는다.
		if (!View.IsOrthographic() && (ShowFlags.IsEnabled(EEngineShowFlag::Fog) || ViewWorld.GetScene().HasFogOverride())) {
			// 2. 안개 Pass
			RenderFogPass(View, ViewWorld, ViewRenderingInfo, ViewCameraLocation, ViewProjection, RenderQueue);
		}

		// 3. ToneMap(HDR) Pass
		RenderToneMapPass(View, ViewWorld, ViewRenderingInfo);

		// 3. FXAA Pass
		if (ShowFlags.IsEnabled(EEngineShowFlag::FXAA))
			RenderFXAAPass(View, ViewRenderingInfo);
	}

	// 4. 오버레이 Pass
	RenderOverlayPass(View, ViewWorld, ViewRenderingInfo, ViewProjection, ViewCameraLocation, ViewCameraForward, RenderQueue, IconQueue, ArrowQueue);

}


// View Texture가 포함된 UI를 Swapchain 백버퍼에 합성한다. Present는 FEngineLoop가 한다.
void UEditorEngine::PresentFrame()
{
	// Swapchain 렌더링
	RenderCommand::BeginRenderPass(MainWindowSC->GetRenderingInfo());

	{
		SCOPE_CYCLE_COUNTER(STAT_ImGui);

		ImGuiRenderer->Begin();

		EditorUI->OnRender();

		ImGuiRenderer->End();
	}

	RenderCommand::EndRenderPass(MainWindowSC->GetRenderingInfo());
}

// ImGui를 정리한다. UObject 일괄 삭제와 공용 자원·Device 정리는 FEngineLoop가 이어서 한다.
void UEditorEngine::PreExit()
{
	EndPlayMap();
	EditorWorld->ClearWorld();
	// PIE 창이 열린 채 종료하면 Device가 살아 있을 때 Swapchain과 타깃을 먼저 정리한다.
	DestroyPIEWindow();
	if (AnimGraphEditorPanel) AnimGraphEditorPanel->ReleaseResources();
	if (StateMachineEditorPanel) StateMachineEditorPanel->ReleaseResources();
	// 탭(애니메이션 미리보기 탭 포함)의 프리뷰 월드는 전역 UObject 일괄 삭제 전에, 타깃은 Device 종료 전에 해제한다.
	// 미리보기 탭을 가리키던 에디터들은 먼저 끊는다.
	if (StateMachineEditorPanel) StateMachineEditorPanel->SetViewer(nullptr);
	if (AnimGraphEditorPanel) AnimGraphEditorPanel->SetViewer(nullptr);
	EditorUI->ReleaseTabResources();
	AnimationPreviewTab = nullptr;
	// Panels outlive the global UObject sweep. Release the owned draft first
	// so its unique_ptr cannot delete an already-destroyed UObject later.
	if (BlendSpaceEditorPanel) BlendSpaceEditorPanel->ReleaseResources();
	ImGuiRenderer->Shutdown();
	ParticleSpriteRenderer.reset();
	ParticleMeshRenderer.reset();
	ParticleStripRenderer.reset();

	Super::PreExit();
}

// 선택과 Gizmo 참조를 정리한 뒤 Actor를 삭제한다.
void UEditorEngine::DeleteActor(AActor* Actor)
{
	if (!Actor)
		return;
	// 실행 중 삭제는 게임 로직이 들고 있는 참조를 깨뜨릴 수 있어 Pause 상태에서만 허용한다.
	if (IsPlaying() && !bIsPaused)
		return;

	OutlinerPanel->SelectActor(nullptr);
	DetailsPanel->SelectComponent(nullptr);

	Actor->Destroy();
}

void UEditorEngine::DeleteComponent(UActorComponent* Component)
{
	if (!Component)
		return;
	if (IsPlaying() && !bIsPaused)
		return;

	AActor* Owner = Component->GetOwner();
	if (!Owner || !Owner->CanDestroyComponent(Component))
		return;

	USceneComponent* NextSelection = nullptr;
	if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
	{
		NextSelection = SceneComponent->GetAttachParent();
		if (NextSelection && NextSelection->GetOwner() != Owner)
			NextSelection = nullptr;
	}

	DetailsPanel->SelectComponent(nullptr);
	Outline->SetTarget(nullptr);
	Owner->DestroyComponent(Component);

	OutlinerPanel->SelectActor(Owner);
	if (NextSelection && NextSelection != Owner->GetRootComponent())
		DetailsPanel->SelectComponent(NextSelection);
}

void UEditorEngine::MakeRootComponent(USceneComponent* NewRoot)
{
	AActor* Owner = NewRoot ? NewRoot->GetOwner() : nullptr;
	if (!Owner || Owner->GetRootComponent() == NewRoot)
		return;
	if (IsPlaying() && !bIsPaused)
		return;

	USceneComponent* OldRoot = Owner->GetRootComponent();

	DetailsPanel->SelectComponent(nullptr);
	Outline->SetTarget(nullptr);
	Owner->ReplaceRootComponent(NewRoot);
	if (Owner->IsDefaultSceneRoot(OldRoot))
		Owner->DestroyComponent(OldRoot);

	OutlinerPanel->SelectActor(Owner);
}

void UEditorEngine::StartPlayInEditorSession()
{
	// 별도 창 모드면 패널 View는 모두 에디터 월드로 두고 창 하나에만 PIE 월드를 그린다.
	// 창을 만들지 못하면 선택한 View에서 실행한다.
	if (PlayMode == EPlayModeType::InEditorFloating && CreatePIEWindow())
		PIEViewport = &ExternalPIEViewport;
	else
		// 지금 편집 중인 View 하나만 PIE 월드를 보여준다.
		PIEViewport = ViewportsPanel->GetEditorViewport();

	FWorldContext& PIEContext = CreateNewWorldContext(EWorldType::PIE);
	PlayWorld = CreatePIEWorldByDuplication(PIEContext, EditorWorld);

	// 게임 카메라는 월드마다 고정 위치에 새로 생기므로, 지금 편집 중인 View의 시점에서 시작하게 맞춘다.
	if (PlayWorld && PlayWorld->GetMainCamera())
	{
		const FViewCamera& EditorCam = ViewportsPanel->GetViewCamera(ViewportsPanel->GetEditorViewIndex());
		if (UCameraComponent* GameCam = PlayWorld->GetMainCamera()->GetCameraComponent())
		{
			GameCam->SetRelativeLocation(EditorCam.Transform.Location);
			GameCam->SetRelativeRotation(EditorCam.Transform.Rotation.ToFRotator());
			GameCam->SetFieldOfView(EditorCam.Projection.FovDegrees);
			GameCam->SetNearZ(EditorCam.Projection.NearClip);
			GameCam->SetFarZ(EditorCam.Projection.FarClip);
			GameCam->SetIsOrthogonal(EditorCam.Projection.Mode == EProjectionMode::Orthographic);
			GameCam->SetOrthoWidth(EditorCam.Projection.OrthoWidth);
		}
	}

	OnActiveWorldChanged();
}

UWorld* UEditorEngine::CreatePIEWorldByDuplication(FWorldContext& PIEContext, UWorld* InEditorWorld)
{
	UWorld* NewPIEWorld = UWorld::GetDuplicatedWorldForPIE(InEditorWorld);
	PIEContext.SetCurrentWorld(NewPIEWorld);
	return NewPIEWorld;
}

void UEditorEngine::OnActiveWorldChanged()
{
	ResetSceneSelection();
	OutlinerPanel->SetWorld(GetActiveWorld());
	SettingsPanel->SetWorld(GetActiveWorld());
}

void UEditorEngine::EndPlayMap()
{
	if (!PlayWorld)
		return;

	ResetSceneSelection();
	if (PIEViewport)
		PIEViewport->GetClient().ClearCameraOverride();
	if (FSceneTransitionManager* SceneTransitions = GetSceneTransitionManager())
		SceneTransitions->CancelForWorld(PlayWorld);
	PlayWorld->EndPlay();
	UIRenderer->ResetUIText();
	UIRenderer->ResetUIImage();
	PlayWorld->CleanupWorld();
	DestroyWorldContext(PlayWorld);
	delete PlayWorld;
	PlayWorld = nullptr;
	PIEViewport = nullptr;
	DestroyPIEWindow();
	OnActiveWorldChanged();
}

bool UEditorEngine::CreatePIEWindow()
{
	constexpr int32 DefaultWidth = 1280;
	constexpr int32 DefaultHeight = 720;

	TUniquePtr<FWindow> NewWindow = MakeUnique<FWindow>();
	if (!NewWindow->Create(::GetModuleHandleW(nullptr), DefaultWidth, DefaultHeight, L"Hitori Engine - PIE"))
	{
		HTR_LOG(Error, "Failed to create PIE window. Falling back to the selected viewport.");
		return false;
	}
	// 닫기 버튼은 PIE만 끝내고, ImGui는 메인 창 입력만 받도록 훅을 끊는다.
	NewWindow->SetQuitOnClose(false);
	NewWindow->SetReceivesWndProcHook(false);

	PIEWindow = std::move(NewWindow);
	PIESwapchain = MakeUnique<FSwapchain>(GetEngineLoop().GetRenderDevice(), PIEWindow.get());
	PIETargetWidth = 0;
	PIETargetHeight = 0;
	PIEWindow->Show();
	return true;
}

void UEditorEngine::DestroyPIEWindow()
{
	if (!PIEWindow)
		return;

	ExternalPIEViewport.SetRect({}, false);
	ExternalPIEViewport.GetClient().ClearCameraOverride();
	ExternalPIEViewport.SetExternalTargets(nullptr, nullptr, nullptr);

	PIERenderingInfo = {};
	PIEColorTarget.reset();
	PIEDepthTarget.reset();
	PIEFxaaTarget.reset();
	PIEHDRTarget.reset();
	PIETargetWidth = 0;
	PIETargetHeight = 0;
	bPIEWindowRendered = false;

	// Swapchain이 창 핸들을 출력 대상으로 쥐고 있으므로 창보다 먼저 놓는다.
	PIESwapchain.reset();
	PIEWindow->Destroy();
	PIEWindow.reset();

	// PIE 창이 포커스를 갖고 있었다면 에디터로 돌려준다.
	::SetForegroundWindow(MainWindow->GetHandle());
}

bool UEditorEngine::UpdatePIEWindowTargets()
{
	const uint32 Width = PIEWindow->GetWidth();
	const uint32 Height = PIEWindow->GetHeight();
	// 최소화 중에는 그릴 대상이 없다.
	if (Width == 0 || Height == 0 || ::IsIconic(PIEWindow->GetHandle()))
	{
		ExternalPIEViewport.SetRect({}, false);
		return false;
	}

	if (PIEWindow->CheckResized())
		PIESwapchain->Resize(Width, Height);

	if (Width != PIETargetWidth || Height != PIETargetHeight)
	{
		// 패널 View 슬롯과 같은 구성이다. Fog가 깊이를 읽으므로 깊이는 TYPELESS로 만든다.
		D3D11_TEXTURE2D_DESC Desc{};
		Desc.Width = Width;
		Desc.Height = Height;
		Desc.MipLevels = 1;
		Desc.ArraySize = 1;
		Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		Desc.SampleDesc.Count = 1;
		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		PIEColorTarget = RenderCommand::CreateTexture2D(Desc);
		PIEFxaaTarget = RenderCommand::CreateTexture2D(Desc);

		Desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		PIEDepthTarget = RenderCommand::CreateTexture2D(Desc);

		Desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		PIEHDRTarget = RenderCommand::CreateTexture2D(Desc);

		PIERenderingInfo.ColorRenderTargets.Reset();
		PIERenderingInfo.ViewportSetting.Width = Width;
		PIERenderingInfo.ViewportSetting.Height = Height;
		FRenderingDesc ColorDesc{};
		ColorDesc.Texture = PIEColorTarget.get();
		PIERenderingInfo.ColorRenderTargets.Add(ColorDesc);
		PIERenderingInfo.DepthStencil.Texture = PIEDepthTarget.get();

		PIETargetWidth = Width;
		PIETargetHeight = Height;
	}

	// 별도 PIE 창 View는 창 크기 전체를 쓰고, 타깃은 창 쪽 것을 빌려 쓴다.
	ExternalPIEViewport.SetRect({ 0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height) }, true);
	ExternalPIEViewport.SetExternalTargets(&PIERenderingInfo, PIEHDRTarget.get(), PIEFxaaTarget.get());
	return true;
}

void UEditorEngine::CopyPIEWindowToBackbuffer()
{
	const FRenderingInfo& BackbufferInfo = PIESwapchain->GetRenderingInfo();
	if (BackbufferInfo.ColorRenderTargets.Num() == 0)
		return;

	// 백버퍼는 SRV가 없어 FXAA·Fog 입력으로 쓸 수 없으므로 오프스크린 결과를 통째로 복사한다.
	// 창 크기 변경 직후 한 프레임은 크기가 어긋날 수 있어 그때는 건너뛴다.
	FTexture2D* Backbuffer = BackbufferInfo.ColorRenderTargets[0].Texture;
	if (!Backbuffer || !PIEColorTarget ||
		BackbufferInfo.ViewportSetting.Width != PIETargetWidth ||
		BackbufferInfo.ViewportSetting.Height != PIETargetHeight)
		return;

	RenderCommand::CopyTexture(Backbuffer, PIEColorTarget.get());
	bPIEWindowRendered = true;
}



// 씬 변경으로 무효화된 에디터의 선택 참조를 모두 해제한다.
void UEditorEngine::ResetSceneSelection()
{
	Gizmo->SetTarget(nullptr, false);
	Outline->SetTarget(nullptr);
	DetailsPanel->SetTarget(nullptr);
	OutlinerPanel->SelectActor(nullptr);
}

// 새 씬 생성이 성공하면 에디터 선택 상태를 초기화한다.
void UEditorEngine::CreateNewScene()
{
	if (IsPlaying())
	{
		HTR_LOG(Warning, "Cannot create a new scene during PIE. Stop the play session first.");
		return;
	}
	if (!FEditorFileUtils::NewScene(EditorWorld))
		return;

	ResetSceneSelection();
}

// 씬 불러오기가 성공하면 에디터 선택 상태를 초기화한다.
void UEditorEngine::OpenScene()
{
	if (IsPlaying())
	{
		HTR_LOG(Warning, "Cannot open a scene during PIE. Stop the play session first.");
		return;
	}
	if (!FEditorFileUtils::LoadScene(EditorWorld))
		return;

	ResetSceneSelection();
}

// 공통 파일 유틸리티로 현재 씬을 저장한다.
void UEditorEngine::SaveCurrentScene()
{
	if (IsPlaying())
	{
		HTR_LOG(Warning, "Cannot save during PIE. Stop the play session first.");
		return;
	}
	FEditorFileUtils::SaveScene(EditorWorld);
}

// 공통 파일 유틸리티로 새 경로에 씬을 저장한다.
void UEditorEngine::SaveSceneAs()
{
	if (IsPlaying())
	{
		HTR_LOG(Warning, "Cannot save during PIE. Stop the play session first.");
		return;
	}
	FEditorFileUtils::SaveSceneAs(EditorWorld);
}


// Script UI coordinates use the same render-target dimensions as the UI renderer.
FGameViewportInfo UEditorEngine::GetGameViewportInfo(const UWorld* InWorld) const
{
    FGameViewportInfo Info;
    if (!InWorld || InWorld != PlayWorld || !PIEViewport)
        return Info;

    if (PIEWindow)
    {
        Info.Size = FVector2(static_cast<float>(PIEWindow->GetWidth()), static_cast<float>(PIEWindow->GetHeight()));
        POINT Mouse{};
        if (::GetCursorPos(&Mouse) && ::ScreenToClient(PIEWindow->GetHandle(), &Mouse))
        {
            Info.MousePosition = FVector2(static_cast<float>(Mouse.x), static_cast<float>(Mouse.y));
            Info.bAcceptsMouseInput = ::GetForegroundWindow() == PIEWindow->GetHandle()
                && Mouse.x >= 0 && Mouse.y >= 0 && Mouse.x < Info.Size.X && Mouse.y < Info.Size.Y;
        }
        return Info;
    }

    if (!ViewportsPanel || !MainWindow || !PIEViewport->IsActive() || !PIEViewport->GetRenderingInfo())
        return Info;

    const FRect& Rect = PIEViewport->GetRect();
    if (Rect.Width <= 0.0f || Rect.Height <= 0.0f)
        return Info;

    const auto& Viewport = PIEViewport->GetRenderingInfo()->ViewportSetting;
    Info.Size = FVector2(static_cast<float>(Viewport.Width), static_cast<float>(Viewport.Height));
    const FVector2 LocalMouse = ViewportsPanel->GetLocalMousePosition();
    const float X = LocalMouse.X - Rect.X;
    const float Y = LocalMouse.Y - Rect.Y;
    Info.MousePosition = FVector2(X * Info.Size.X / Rect.Width, Y * Info.Size.Y / Rect.Height);
    Info.bAcceptsMouseInput = ViewportsPanel->IsHovered() && !ImGui::GetIO().WantTextInput
        && ::GetForegroundWindow() == MainWindow->GetHandle()
        && X >= 0.0f && Y >= 0.0f && X < Rect.Width && Y < Rect.Height;
    return Info;
}

void UEditorEngine::RequestQuitGame(const UWorld* InWorld)
{
    // A game menu closes the PIE session, not the editor and its unsaved scene.
    if (InWorld && InWorld == PlayWorld)
        RequestEndPlayMap();
}

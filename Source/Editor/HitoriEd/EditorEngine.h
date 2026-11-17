#pragma once

#include "Engine/Engine.h"
#include "Core/Window.h"
#include "Core/Types.h"

#include "Render/Renderer.h"
#include "Render/RenderDevice.h"
#include "Render/Swapchain.h"
#include "Editor/EditorUI/ImGuiRenderer.h"
#include "Editor/Rendering/GridRenderer.h"
#include "Editor/Gizmo/GizmoRenderer.h"
#include "Render/LineBatcher.h"
#include "Render/FogRenderer.h"
#include "Render/FXAARenderer.h"
#include "Render/ToneMapRenderer.h"
#include "Render/DepthViewRenderer.h"
#include "Render/FShadowViewRenderer.h"
#include "Render/UIRenderer.h"
#include "Render/DecalRenderer.h"
#include "Render/ParticleSpriteRenderer.h"
#include "Render/ParticleMeshRenderer.h"


#include "Editor/EditorUI/EditorUI.h"
#include "Editor/OutputLog/OutputLogPanel.h"
#include "Editor/Details/DetailsPanel.h"
#include "Editor/EditorControls/EditorControlsPanel.h"
#include "Editor/Settings/SettingsPanel.h"
#include "Editor/Viewports/ViewportsPanel.h"
#include "Editor/HitoriEd/EditorSceneView.h"
#include "Editor/ContentDrawer/ContentDrawerPanel.h"

#include "Editor/Rendering/Outline.h"
#include "Editor/Rendering/OutLineRenderer.h"
#include "Editor/Outliner/OutlinerPanel.h"

#include "Render/SkyboxRenderer.h"

#include "PlayInEditorDataTypes.h"
#include "Core/Misc/Optional.h"

#include "Core/CoroutineScheduler.h"

//Temp
#include "Text/Font.h"
#include "Text/TextRenderer.h"

class FSkeletalMeshViewerPanel;
class FBlendSpaceEditorPanel;
class FStateMachineEditorPanel;
class FAnimGraphEditorPanel;
class UParticleSystem;

class UEditorEngine : public UEngine
{
	DECLARE_CLASS(UEditorEngine, UEngine)

public:
	FEngineConfig GetConfig() const override;
	bool Init() override;
	void Tick(float DeltaTime) override;
	void PreExit() override;
	FUIRenderer* GetUIRenderer() override { return UIRenderer.get(); }
    FGameViewportInfo GetGameViewportInfo(const UWorld* InWorld) const override;
    void RequestQuitGame(const UWorld* InWorld) override;

	// Active View의 입력과 Picking 결과만 Gizmo 및 선택 상태에 반영한다.
	void UpdateGizmoAndPicking();
	// View 하나의 Scene·Grid·Gizmo·텍스트를 해당 ViewProjection으로 렌더한다.
	// 어떤 월드를 어떤 View로 어디에 그릴지는 호출자가 정한다. 레벨 뷰포트·PIE 창·애셋 에디터 프리뷰가 함께 쓴다.
	void RenderFrame(const FEditorSceneView& View, UWorld& ViewWorld, const FRenderingInfo& ViewRenderingInfo, FRenderQueue& RenderQueue, FRenderQueue& IconQueue, FRenderQueue& ArrowQueue);
	// 네 View 결과와 ImGui를 메인 Swapchain 백버퍼에 합성한다. Present는 FEngineLoop가 한다.
	void PresentFrame();
	void DeleteActor(AActor* Actor);
	void DeleteComponent(UActorComponent* Component);
	void MakeRootComponent(USceneComponent* NewRoot);

	//Play 버튼을 눌렀을때 Play Session 실행을 요청한다.
	inline void RequestPlaySession() { bPlaySessionRequested = true; }
	inline void RequestEndPlayMap() { bRequestEndPlayMapQueued = true; bIsPaused = false; }

	//실제 PIE 를 실행
	void StartPlayInEditorSession();
	UWorld* CreatePIEWorldByDuplication(FWorldContext& PIEContext, UWorld* InEditorWorld);

	void OnActiveWorldChanged();

	void EndPlayMap();

	bool IsPlaying() const { return PlayWorld != nullptr; }

	////FEditorDelegates::PrePIEEnded / EndPIE.Broadcast()
	//void TeardownPlaySession(const FWorldContext& PIEContext);            // 액터 EndPlay, 월드 정리(CleanupWorld)
	//void RestoreEditorWorld(UWorld* EditorWorld);            // GWorld를 에디터 월드로 복구
	//void DestroyWorldContext(UWorld* PlayWorld);

private:
	bool ExecShadowCommand(const FString& CommandLine, FString& OutMessage);
	bool ExecShadowResolutionCommand(const FString& CommandLine, FString& OutMessage);
	// 이번 프레임 DeltaTime을 패널에 전달하고 에디터 단축키를 처리한다.
	void BeginFrame(float DeltaTime);
	// 패널의 레이아웃·Preset 요청과 카메라 입력을 레벨 View에 반영한다.
	void UpdateMultipleViewportState(float DeltaTime);
	// 월드를 정확히 한 번 Tick·Capture한 뒤 에디터 상호작용을 갱신한다.
	void TickWorldAndEditor(float DeltaTime);
	// 보이는 탭이 내놓은 화면(레벨 View·에셋 에디터 프리뷰)과 별도 PIE 창을 한 경로로 그린다.
	void RenderAllViews();
	// 레벨 View 하나를 RenderAllViews의 입력 형식으로 만든다.
	FEditorTabView MakeLevelTabView(const FViewport& Viewport) const;
	// 레벨 탭의 CollectViews가 부른다. 그릴 수 있는 패널 View만 넣는다.
	void CollectLevelViews(TArray<FEditorTabView>& OutViews) const;
	// PIE View가 PlayWorld 메인 카메라로 보이도록 카메라 Override를 갱신하고 게임 카메라 입력을 켜고 끈다.
	void UpdatePIEViewCamera();

	// 별도 PIE 창(New Editor Window)을 만들고 정리한다.
	bool CreatePIEWindow();
	void DestroyPIEWindow();
	// 창 크기에 맞춰 Swapchain과 오프스크린 타깃을 다시 만든다. 그릴 수 없으면 false다.
	bool UpdatePIEWindowTargets();
	// PIE 창의 오프스크린 결과를 백버퍼로 복사한다. Present는 EndFrame에서 한다.
	void CopyPIEWindowToBackbuffer();
	bool IsPIEInWindow() const { return PIEWindow != nullptr; }
	// 화면 합성과 View 설정 보관으로 프레임을 마무리한다.
	void EndFrame();

	//불투명 물체에 대한 Pass
	void RenderOpaquePass(const FEditorSceneView& View, const FRenderingInfo& ViewRenderingInfo,	const FViewConstants& ViewConstants, FRenderQueue& RenderQueue, UWorld& ViewWorld);
	//안개에 대한 Pass
	void RenderFogPass(const FEditorSceneView& View, UWorld& ViewWorld, const FRenderingInfo& ViewRenderingInfo, const FVector& ViewCameraLocation,const FMatrix& ViewProjection, FRenderQueue& RenderQueue);
	//Overlay Pass
	void RenderOverlayPass(const FEditorSceneView& View, UWorld& ViewWorld, const FRenderingInfo& ViewRenderingInfo, const FMatrix& ViewProjection, const FVector& ViewCameraLocation, const FVector& ViewCameraForward, FRenderQueue& RenderQueue, FRenderQueue& IconQueue, FRenderQueue& ArrowQueue);
	//FXAA Pass
	void RenderFXAAPass(const FEditorSceneView& View, const FRenderingInfo& ViewRenderingInfo);
	//Depth Pass
	void RenderDepthPass(const FEditorSceneView& View, const FRenderingInfo& ViewRenderingInfo, const FMatrix& ViewProjection, const FVector& ViewCameraLocation, const FVector& ViewCameraForward);
	//Normal Pass
	void RenderNormalPass(const int32 ViewIndex, const FRenderingInfo& ViewRenderingInfo, const FMatrix& ViewProjection, const FVector& ViewCameraLocation, const FVector& ViewCameraForward);
	//ToneMap Pass
	void RenderToneMapPass(const FEditorSceneView& View, UWorld& ViewWorld, const FRenderingInfo& ViewRenderingInfo);

	// 레벨 View(패널 넷과 별도 PIE 창)의 상태와 타깃을 RenderFrame 입력 하나로 묶는다.
	FEditorSceneView MakeLevelSceneView(const FViewport& Viewport) const;
	// 그림자 View ID로 레벨 View(패널 넷과 별도 PIE 창)를 찾는다. 없으면 nullptr이다.
	FViewport* FindLevelViewportByShadowId(uint32 ShadowViewId);
	// 이번 프레임에 그릴 수 있는 레벨 View인가. 별도 PIE 창 View는 게임 카메라가 있어야 한다.
	bool IsLevelViewRenderable(const FViewport& Viewport) const;

	// 파티클 에디터 탭을 연다. 같은 에셋을 편집 중인 탭이 있으면 새로 만들지 않고 그 탭을 앞으로 가져온다.
	void OpenParticleSystemEditor(UParticleSystem* Asset);
	// 콘텐츠 드로어의 .particle 더블클릭. 파일을 불러온 뒤 OpenParticleSystemEditor로 넘긴다.
	void OpenParticleSystemFile(const FString& Path);

	//Icon Pass
	void RenderIconPass(int32 ViewIndex, UWorld& ViewWorld, const FRenderingInfo& ViewRenderingInfo, const FMatrix& ViewProjection, const FVector& ViewCameraLocation, const FVector& ViewCameraForward, FRenderQueue& RenderIconQueue, FRenderQueue& ArrowQueue);

	FWorldContext& GetEditorWorldContext();
	FWorldContext* GetPIEWorldContext(int32 WorldPIEInstance = 0);
	UWorld* GetActiveWorld() const { return PlayWorld ? PlayWorld : EditorWorld; }
	// PIE를 시작한 View만 PlayWorld를, 나머지 View는 EditorWorld를 그린다.
	UWorld* GetWorldForView(const FViewport* Viewport) const { return PlayWorld && Viewport && Viewport == PIEViewport ? PlayWorld : EditorWorld; }
	// PIE 중이고 일시정지가 아닌 View는 Gizmo·Picking 등 에디터 상호작용을 하지 않는다.
	bool IsPlayingView(const FViewport* Viewport) const { return PlayWorld && Viewport && Viewport == PIEViewport && !bIsPaused; }

	// FEngineLoop 소유. OnInit에서 받아 둔다.
	FWindow* MainWindow = nullptr;
	FSwapchain* MainWindowSC = nullptr;
	FRenderer* Renderer = nullptr;

	TUniquePtr<FEditorUI> EditorUI;

	TUniquePtr<FImGuiRenderer> ImGuiRenderer;
	TUniquePtr<FGridRenderer> GridRenderer;
	TUniquePtr<FGizmoRenderer> GizmoRenderer;
	TUniquePtr<FTextRenderer> TextRenderer;
	TUniquePtr<FFogRenderer> FogRenderer;
	TUniquePtr<FDecalRenderer> DecalRenderer;
	TUniquePtr<FLineBatcher> LineBatcher;
	TUniquePtr<FGizmo> Gizmo;
	TUniquePtr<FOutline> Outline;
	TUniquePtr<FOutlineRenderer> OutlineRenderer;
	TUniquePtr<FSkyboxRenderer> SkyboxRenderer;
	TUniquePtr<FFXAARenderer> FXAARenderer;
	TUniquePtr<FToneMapRenderer> ToneMapRenderer;
	TUniquePtr<FDepthViewRenderer> DepthViewRenderer;
	TUniquePtr<FShadowViewRenderer> ShadowViewRenderer;
	TUniquePtr<FUIRenderer> UIRenderer;
	TUniquePtr<FParticleSpriteRenderer> ParticleSpriteRenderer;
	TUniquePtr<FParticleMeshRenderer> ParticleMeshRenderer;

	UFont* SystemFont;

	FOutputLogPanel* OutputLogPanel = nullptr;

	FDetailsPanel* DetailsPanel = nullptr;
	FEditorControlsPanel* EditorControlsPanel = nullptr;
	FSettingsPanel* SettingsPanel = nullptr;
	FViewportsPanel* ViewportsPanel = nullptr;
	// 소유(합성): 별도 PIE 창의 View. 타깃은 PIE 창 쪽(PIEColorTarget 등)을 빌려 쓴다.
	FViewport ExternalPIEViewport{ ExternalPIEShadowViewId };
	FRenderQueue RenderQueue;
	FRenderQueue IconQueue;		// Light Icon Queue
	FRenderQueue ArrowQueue;
	FOutlinerPanel* OutlinerPanel = nullptr;
	FContentDrawerPanel* ContentDrawerPanel = nullptr;
	FBlendSpaceEditorPanel* BlendSpaceEditorPanel = nullptr;
 FAnimGraphEditorPanel* AnimGraphEditorPanel = nullptr;
 FStateMachineEditorPanel* StateMachineEditorPanel = nullptr;
	FSkeletalMeshViewerPanel* SkeletalMeshViewerPanel = nullptr;

	UWorld* EditorWorld = nullptr;
	UWorld* PlayWorld = nullptr;
	// 참조(집약): PlayWorld를 그리는 View. 패널 View 하나이거나 ExternalPIEViewport다. PIE가 아니면 nullptr이다.
	FViewport* PIEViewport = nullptr;

	// Play 버튼이 다음 세션을 띄울 위치
	EPlayModeType PlayMode = EPlayModeType::InViewport;

	// New Editor Window(PIE) 모드에서만 존재한다. Swapchain이 창 핸들을 쓰므로 창보다 먼저 해제한다.
	TUniquePtr<FWindow> PIEWindow;
	TUniquePtr<FSwapchain> PIESwapchain;
	TUniquePtr<FTexture2D> PIEColorTarget;
	TUniquePtr<FTexture2D> PIEDepthTarget;
	TUniquePtr<FTexture2D> PIEFxaaTarget;
	TUniquePtr<FTexture2D> PIEHDRTarget;
	FRenderingInfo PIERenderingInfo{};
	uint32 PIETargetWidth = 0;
	uint32 PIETargetHeight = 0;
	bool bPIEWindowRendered = false;

	FVector UUIDLocation;

	FEditorCommand Save;

	FEditorCommand Play;
	FEditorCommand Pause;
	FEditorCommand Resume;
	FEditorCommand StepFrame;
	FEditorCommand Stop;

	bool bPlaySessionRequested = false;
	bool bRequestEndPlayMapQueued = false;
	bool bStepRequested = false;
	bool bIsPaused = false;

	void ResetSceneSelection();

	void CreateNewScene();
	void OpenScene();
	void SaveCurrentScene();
	void SaveSceneAs();

};

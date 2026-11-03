#include "EnginePCH.h"
#include "Editor/Viewports/SkeletalMeshViewerPanel.h"
#include "Engine/World.h"
#include "GameFramework/Actor/SkeletalMeshActor.h"
#include "GameFramework/Actor/DirectionalLightActor.h"
#include "Components/PrimitiveComponent.h"

#include <algorithm>

bool FSkeletalMeshViewerPanel::Init()
{
	// 에디터 엔진이 EditorWorld로 쥐지 않으므로 아웃라이너·씬 저장·PIE 복제에 잡히지 않는다
	PreviewWorld = UWorld::CreateWorld(EWorldType::SkeletalMeshViewer, false, "SkeletalMeshPreview");
	PreviewActor = PreviewWorld->SpawnActor<ASkeletalMeshActor>();

	// 라이트 아이콘과 화살표 메시는 프리뷰에 방해만 되니 숨기고 빛만 남긴다
	ADirectionalLightActor* Light = PreviewWorld->SpawnActor<ADirectionalLightActor>();
	for (UActorComponent* Component : Light->GetComponents())
	{
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
			Primitive->SetVisible(false);
	}

	ViewportClient = MakeUnique<FEditorViewportClient>();
	ViewportClient->SetWorld(PreviewWorld);
	ViewportClient->SetViewIndex(4); // 0~3은 메인 뷰포트 몫
	ViewportClient->Resize(1, 1);
	FocusCamera();
	return true;
}

void FSkeletalMeshViewerPanel::Tick(float DeltaTime)
{
	(void)DeltaTime;
	if (!ViewportClient)
		return;
	if (RequestedWidth != ViewportClient->GetWidth() || RequestedHeight != ViewportClient->GetHeight())
		ViewportClient->Resize(RequestedWidth, RequestedHeight);
}

void FSkeletalMeshViewerPanel::OnRender()
{
	if (!IsOpen() || !ViewportClient)
	{
		bHovered = false;
		bCameraCaptured = false;
		if (ViewportClient)
			ViewportClient->SetActive(false);
		return;
	}

	bool bOpen = true;
	bHovered = false;
	if (ImGui::Begin(GetPanelName(), &bOpen))
	{
		const ImVec2 Size = ImGui::GetContentRegionAvail();
		const bool bVisible = Size.x >= 1.0f && Size.y >= 1.0f;
		ViewportClient->SetActive(bVisible);
		if (bVisible)
		{
			// 렌더는 다음 Tick 뒤에 이 크기로 한다. 이번 프레임은 지난 프레임 결과를 늘려 보여 준다
			RequestedWidth = static_cast<uint32>(Size.x);
			RequestedHeight = static_cast<uint32>(Size.y);
			ViewportClient->SetRect({ 0.0f, 0.0f, Size.x, Size.y });
			if (FTexture2D* Color = ViewportClient->GetColorTarget(); Color && Color->GetSRV())
				ImGui::Image(Color->GetSRV(), Size);
			else
				ImGui::Dummy(Size);
			bHovered = ImGui::IsItemHovered();
		}
	}
	else
	{
		// 탭이 가려졌거나 접힌 상태
		ViewportClient->SetActive(false);
	}
	GatherCameraInput();
	ImGui::End();
	if (!bOpen)
	{
		SetOpen(false);
		ViewportClient->SetActive(false);
	}
}

void FSkeletalMeshViewerPanel::OpenAsset(USkeletalMesh* Mesh)
{
	if (!PreviewActor || !Mesh)
		return;
	PreviewActor->GetSkeletalMeshComponent()->SetSkeletalMesh(Mesh);
	FocusCamera();
	SetOpen(true);
	ImGui::SetWindowFocus(GetPanelName());
}

void FSkeletalMeshViewerPanel::Shutdown()
{
	// UObject 해제는 FEngineLoop가 종료 때 일괄로 한다. 여기서는 컴포넌트 등록과 씬 프록시만 정리한다
	if (PreviewWorld)
		PreviewWorld->ClearWorld();
	PreviewWorld = nullptr;
	PreviewActor = nullptr;
	ViewportClient.reset();
}

void FSkeletalMeshViewerPanel::GatherCameraInput()
{
	// FInputSystem은 메인 창 메시지만 받아서, 메인 창 밖으로 꺼낸 ImGui 창에서는 입력이 오지 않는다.
	// ImGui IO는 모든 ImGui 창의 입력을 받으므로 뷰어 카메라는 이것으로 움직인다
	const ImGuiIO& IO = ImGui::GetIO();
	if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		bCameraCaptured = true;
		ImGui::SetWindowFocus(GetPanelName());
	}
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) || !ViewportClient->IsActive())
		bCameraCaptured = false;

	CameraInput.bCaptured = bCameraCaptured;
	CameraInput.bAllowWheel = bCameraCaptured || bHovered;
	if (bCameraCaptured)
	{
		CameraInput.MouseX += IO.MouseDelta.x;
		CameraInput.MouseY += IO.MouseDelta.y;
	}
	// ImGui 휠은 한 칸이 1.0, FInputSystem 휠은 한 칸이 WHEEL_DELTA(120)라 메인 뷰포트와 속도를 맞춘다
	if (CameraInput.bAllowWheel)
		CameraInput.Wheel += IO.MouseWheel * static_cast<float>(WHEEL_DELTA);
	CameraInput.bForward = ImGui::IsKeyDown(ImGuiKey_W);
	CameraInput.bBackward = ImGui::IsKeyDown(ImGuiKey_S);
	CameraInput.bRight = ImGui::IsKeyDown(ImGuiKey_D);
	CameraInput.bLeft = ImGui::IsKeyDown(ImGuiKey_A);
	CameraInput.bUp = ImGui::IsKeyDown(ImGuiKey_E);
	CameraInput.bDown = ImGui::IsKeyDown(ImGuiKey_Q);
}

FViewportCameraInput FSkeletalMeshViewerPanel::ConsumeCameraInput()
{
	const FViewportCameraInput Result = CameraInput;
	CameraInput.MouseX = 0.0f;
	CameraInput.MouseY = 0.0f;
	CameraInput.Wheel = 0.0f;
	return Result;
}

void FSkeletalMeshViewerPanel::FocusCamera()
{
	if (!PreviewActor || !ViewportClient)
		return;
	const FBox Bounds = PreviewActor->GetSkeletalMeshComponent()->CalcBounds();
	const FVector Center = (Bounds.Min + Bounds.Max) * 0.5f;
	const FVector Size = Bounds.Max - Bounds.Min;
	const float Radius = std::max({ Size.X, Size.Y, Size.Z, 1.0f });
	// X가 앞이라 -X 쪽에서 +X를 바라본다. 거리는 화각을 따지지 않은 어림값
	ViewportClient->SetViewLocation(Center - FVector(Radius * 1.5f, 0.0f, 0.0f));
	ViewportClient->SetViewRotation(FRotator(0.0f, 0.0f, 0.0f));
}

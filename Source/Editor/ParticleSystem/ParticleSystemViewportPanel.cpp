#include "EnginePCH.h"
#include "Editor/ParticleSystem/ParticleSystemViewportPanel.h"
#include "Editor/ParticleSystem/ParticleEditorContext.h"

#include "Camera/CameraComponent.h"
#include "Render/RenderCommand.h"

#include <algorithm>

namespace
{
	// 프리뷰 카메라 조작 속도. 레벨 뷰포트 설정(FSettings)의 기본값과 같다.
	constexpr float PreviewCameraMoveSpeed = 1.0f;
	constexpr float PreviewCameraMouseSensitivity = 1.0f;
	// ImGui 휠 한 칸을 Client가 기대하는 원시 휠 값(WHEEL_DELTA)으로 바꾼다.
	constexpr float RawWheelDeltaPerStep = 120.0f;
}

FParticleSystemViewportPanel::~FParticleSystemViewportPanel()
{
	ReleaseResources();
}

bool FParticleSystemViewportPanel::Init()
{
	ResetCamera();
	// 스프라이트는 조명이 필요 없다. 프리뷰 월드에는 라이트도 없다.
	ViewportClient.SetViewMode(EViewMode::Unlit);
	return true;
}

void FParticleSystemViewportPanel::ResetCamera()
{
	// 투영 수치는 레벨 View와 같은 방식으로 프리뷰 월드 기본 카메라에서 가져온다.
	FCameraProjection Projection{ EProjectionMode::Perspective, 60.0f, 10.0f, 0.1f, 1000.0f };
	if (UWorld* World = Context.GetPreviewWorld())
	{
		if (const UCameraComponent* MainCamera = World->GetMainCameraComponent())
		{
			Projection.FovDegrees = MainCamera->GetFieldOfView();
			Projection.OrthoWidth = MainCamera->GetOrthoWidth();
			Projection.NearClip = MainCamera->GetNearZ();
			Projection.FarClip = MainCamera->GetFarZ();
		}
	}

	// 레벨 뷰포트의 초기 구도와 같은 방향에서 원점(프리뷰 액터)을 조금 더 가까이 본다.
	const FViewCamera InitialCamera{ {{-5.8f, 3.0f, 5.0f},
		{0.081251f, 0.256328f, -0.291035f, 0.918146f}}, Projection };
	const FRect Rect = TargetWidth > 0 ? GetViewRect() : FRect{ 0.0f, 0.0f, 800.0f, 600.0f };
	ViewportClient.SetCamera(InitialCamera, Rect);
}

void FParticleSystemViewportPanel::OnRender()
{
	bool bOpen = IsOpen();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	bWindowVisible = ImGui::Begin(GetPanelName(), &bOpen);
	ImGui::PopStyleVar();

	bViewportHovered = false;
	if (bWindowVisible)
	{
		const ImVec2 Available = ImGui::GetContentRegionAvail();
		RequestedWidth = static_cast<uint32>(std::max(Available.x, 0.0f));
		RequestedHeight = static_cast<uint32>(std::max(Available.y, 0.0f));

		// 첫 프레임은 타깃이 아직 없다. 크기만 요청하고 다음 프레임부터 그린다.
		if (ColorTarget && TargetWidth > 0 && TargetHeight > 0)
		{
			// 타깃은 한 프레임 전 크기일 수 있으므로 현재 영역에 맞춰 늘려 그린다.
			ImGui::Image(ColorTarget->GetSRV(), Available);
			bViewportHovered = ImGui::IsItemHovered();
		}
		UpdateCameraInput();
	}
	else
	{
		bCameraCaptured = false;
	}
	ImGui::End();

	SetOpen(bOpen);
}

void FParticleSystemViewportPanel::UpdateCameraInput()
{
	// 우클릭을 뷰포트 위에서 시작했을 때만 Capture한다. 드래그 중 창 밖으로 나가도 유지한다.
	if (bViewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		bCameraCaptured = true;
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
		bCameraCaptured = false;

	if (TargetWidth == 0 || TargetHeight == 0)
		return;

	const ImGuiIO& IO = ImGui::GetIO();
	FCameraMoveInput RawInput{};
	if (bCameraCaptured)
	{
		const auto AxisValue = [](ImGuiKey Positive, ImGuiKey Negative)
		{
			return (ImGui::IsKeyDown(Positive) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(Negative) ? 1.0f : 0.0f);
		};
		RawInput.MoveAxis = {
			AxisValue(ImGuiKey_W, ImGuiKey_S),
			AxisValue(ImGuiKey_D, ImGuiKey_A),
			AxisValue(ImGuiKey_E, ImGuiKey_Q)
		};
		RawInput.MouseDelta = { IO.MouseDelta.x, IO.MouseDelta.y };
	}
	if (bCameraCaptured || bViewportHovered)
		RawInput.ZoomDelta = IO.MouseWheel * RawWheelDeltaPerStep;

	ViewportClient.UpdateCamera(LastDeltaTime, RawInput, GetViewRect(),
		PreviewCameraMoveSpeed, PreviewCameraMouseSensitivity, bCameraCaptured);
}

bool FParticleSystemViewportPanel::PreparePreviewRender()
{
	if (!IsOpen() || !bWindowVisible || !Context.GetPreviewWorld())
		return false;
	if (RequestedWidth == 0 || RequestedHeight == 0)
		return false;

	if (RequestedWidth != TargetWidth || RequestedHeight != TargetHeight)
		ResizeTargets(RequestedWidth, RequestedHeight);

	return ColorTarget && DepthTarget && HDRTarget;
}

void FParticleSystemViewportPanel::ResizeTargets(const uint32 Width, const uint32 Height)
{
	// 레벨 View 슬롯과 같은 구성이다. Fog·Depth View가 깊이를 읽으므로 깊이는 TYPELESS로 만든다.
	D3D11_TEXTURE2D_DESC Desc{};
	Desc.Width = Width;
	Desc.Height = Height;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	ColorTarget = RenderCommand::CreateTexture2D(Desc);
	FxaaTarget = RenderCommand::CreateTexture2D(Desc);

	Desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	DepthTarget = RenderCommand::CreateTexture2D(Desc);

	Desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	HDRTarget = RenderCommand::CreateTexture2D(Desc);

	RenderingInfo.ColorRenderTargets.Reset();
	RenderingInfo.ViewportSetting.Width = Width;
	RenderingInfo.ViewportSetting.Height = Height;
	FRenderingDesc ColorDesc{};
	ColorDesc.Texture = ColorTarget.get();
	// 스카이박스 없이 그리므로 배경은 이 지우기 색이다.
	ColorDesc.ClearValue.ColorClearValue = { 0.08f, 0.08f, 0.09f, 1.0f };
	RenderingInfo.ColorRenderTargets.Add(ColorDesc);
	RenderingInfo.DepthStencil.Texture = DepthTarget.get();

	TargetWidth = Width;
	TargetHeight = Height;
	// Aspect가 바뀌면 직교 폭 보정이 필요하다.
	ViewportClient.OnViewportResized(GetViewRect());
}

void FParticleSystemViewportPanel::ReleaseResources()
{
	RenderingInfo = {};
	ColorTarget.reset();
	DepthTarget.reset();
	HDRTarget.reset();
	FxaaTarget.reset();
	TargetWidth = TargetHeight = 0;
	RequestedWidth = RequestedHeight = 0;
}

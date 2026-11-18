#include "EnginePCH.h"
#include "Editor/Viewports/Viewport.h"

#include "Engine/World.h"
#include "Asset/LOD/StaticMeshLODSelector.h"
#include "Render/RenderPacket.h"
#include "Render/RenderCommand.h"

#include <algorithm>

void FViewport::SetRect(const FRect& InRect, const bool bInActive)
{
	Rect = InRect;
	bActive = bInActive && IsViewRectValid(InRect);
	if (!bActive)
		return;

	// 창 크기와 Single/Quad 전환으로 Aspect가 바뀌어도 저장값과 실제 투영값을 함께 보정한다.
	Client.OnViewportResized(Rect);

	if (!bOwnsRenderTargets)
		return;
	const uint32 Width = static_cast<uint32>(std::max(1.0f, Rect.Width));
	const uint32 Height = static_cast<uint32>(std::max(1.0f, Rect.Height));
	if (Width != TargetWidth || Height != TargetHeight)
		ResizeTargets(Width, Height);
}

void FViewport::SetExternalTargets(const FRenderingInfo* InRenderingInfo, FTexture2D* InHDRTarget, FTexture2D* InFxaaTarget)
{
	ExternalRenderingInfo = InRenderingInfo;
	ExternalHDRTarget = InHDRTarget;
	ExternalFxaaTarget = InFxaaTarget;
}

const FRenderingInfo* FViewport::GetRenderingInfo() const
{
	return bOwnsRenderTargets ? &RenderingInfo : ExternalRenderingInfo;
}

FTexture2D* FViewport::GetHDRTarget() const
{
	return bOwnsRenderTargets ? HDRTarget.get() : ExternalHDRTarget;
}

FTexture2D* FViewport::GetFxaaTarget() const
{
	return bOwnsRenderTargets ? FxaaTarget.get() : ExternalFxaaTarget;
}

void FViewport::ReleaseRenderTargets()
{
	RenderingInfo = {};
	ColorTarget.reset();
	DepthTarget.reset();
	FxaaTarget.reset();
	HDRTarget.reset();
	TargetWidth = TargetHeight = 0;
}

// View 크기에 맞춰 Color·Depth·HDR·FXAA Texture와 렌더 정보를 재생성한다.
void FViewport::ResizeTargets(const uint32 Width, const uint32 Height)
{
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
	// CopyResource로 되돌려 쓰므로 ColorTarget과 같은 Desc로 만든다
	FxaaTarget = RenderCommand::CreateTexture2D(Desc);

	// Fog 패스가 깊이를 셰이더에서 읽으므로 DSV와 SRV를 둘 다 만들 수 있는 TYPELESS로 만든다
	Desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	DepthTarget = RenderCommand::CreateTexture2D(Desc);

	Desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	HDRTarget = RenderCommand::CreateTexture2D(Desc);

	TargetWidth = Width;
	TargetHeight = Height;
	RenderingInfo.ColorRenderTargets.Reset();
	RenderingInfo.ViewportSetting.Width = Width;
	RenderingInfo.ViewportSetting.Height = Height;
	FRenderingDesc ColorDesc{};
	ColorDesc.Texture = ColorTarget.get();
	RenderingInfo.ColorRenderTargets.Add(ColorDesc);
	RenderingInfo.DepthStencil.Texture = DepthTarget.get();
}

FMatrix FViewport::GetViewProjection() const
{
	return FEditorViewportClient::CalcViewProjection(GetEffectiveCamera(), Rect);
}

FMatrix FViewport::GetViewMatrix() const
{
	FMatrix View, Projection;
	FEditorViewportClient::CalcViewMatrices(GetEffectiveCamera(), Rect, View, Projection);
	return View;
}

FMatrix FViewport::GetProjectionMatrix() const
{
	FMatrix View, Projection;
	FEditorViewportClient::CalcViewMatrices(GetEffectiveCamera(), Rect, View, Projection);
	return Projection;
}

FLODViewContext FViewport::GetLODViewContext() const
{
	return FEditorViewportClient::CalcLODViewContext(GetEffectiveCamera(), Rect);
}

bool FViewport::AppendShadowFrustumCorners(const float ShadowDistance, TArray<FVector>& OutCorners) const
{
	return bActive && FEditorViewportClient::AppendShadowFrustumCorners(GetEffectiveCamera(), Rect, ShadowDistance, OutCorners);
}

bool FViewport::AppendShadowFrustumCorners(const float NearDistance, const float FarDistance, TArray<FVector>& OutCorners) const
{
	return bActive && FEditorViewportClient::AppendShadowFrustumCorners(GetEffectiveCamera(), Rect, NearDistance, FarDistance, OutCorners);
}

FVector FViewport::GetCameraForward() const
{
	return FEditorViewportClient::NormalizedOrZero(GetEffectiveCamera().Transform.Rotation.GetForwardVector());
}

FVector FViewport::GetCameraUp() const
{
	return FEditorViewportClient::NormalizedOrZero(GetEffectiveCamera().Transform.Rotation.GetUpVector());
}

FMatrix FViewport::BuildBillboardMatrix(const FVector& WorldPosition, const float Width, const float Height) const
{
	return FEditorViewportClient::BuildBillboardMatrix(GetEffectiveCamera(), WorldPosition, Width, Height);
}

FRay FViewport::Deproject(const FVector2 ViewLocalPosition) const
{
	return FEditorViewportClient::DeprojectView(GetEffectiveCamera(), Rect, ViewLocalPosition);
}

void FViewport::BuildRenderQueue(UWorld& World, FRenderQueue& OutQueue, FRenderQueue& OutIconQueue, FRenderQueue& OutArrowQueue) const
{
	if (!bActive)
	{
		OutQueue.Reset();
		OutIconQueue.Reset();
		OutArrowQueue.Reset();
		return;
	}
	BuildRenderQueue(World, GetEffectiveCamera(), Rect, OutQueue, OutIconQueue, OutArrowQueue);
}

void FViewport::BuildRenderQueue(UWorld& World, const FViewCamera& ViewCamera, const FRect& ViewRect,
	FRenderQueue& OutQueue, FRenderQueue& OutIconQueue, FRenderQueue& OutArrowQueue)
{
	OutQueue.Reset();
	OutIconQueue.Reset();
	OutArrowQueue.Reset();

	// Runtime은 이 동기 호출 동안만 논리 카메라 Context를 사용한다.
	const auto ResolveBillboard = [](const FVector& Position, float Width, float Height, const void* Context) -> FMatrix
	{
		const auto& Camera = *static_cast<const FViewCamera*>(Context);
		return FEditorViewportClient::BuildBillboardMatrix(Camera, Position, Width, Height);
	};

	FLODViewContext LODContext = FEditorViewportClient::CalcLODViewContext(ViewCamera, ViewRect);
	FFrustumPlanes Frustum = ExtractFrustumPlanes(LODContext.ViewProjection);

	// Renderer를 넘기지 않아 기존 에디터의 GPU 오클루전·상수 업로드 정책을 유지한다.
	World.GatherRenderPackets(OutQueue, &LODContext, &Frustum, nullptr,
		ResolveBillboard, &ViewCamera, &OutIconQueue, &OutArrowQueue);
}

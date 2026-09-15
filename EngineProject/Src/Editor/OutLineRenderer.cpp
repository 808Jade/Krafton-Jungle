#include "EnginePCH.h"
#include "OutLineRenderer.h"
#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"

void FOutlineRenderer::Init(FRenderer* InRenderer)
{
	Renderer = InRenderer;
	Shader = RenderCommand::CreateShader(L"Assets/Shader/OutlineShader.hlsl", FVertex::GetLayout());

	ConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FOutlineData));
}

void FOutlineRenderer::SetMesh(UStaticMesh* InMesh)
{
	Mesh = InMesh;
}

void FOutlineRenderer::OnRender(const FOutline& InOutline, const FMatrix& InViewProj, const FVector4& CameraPos)
{
	if (!InOutline.GetTarget())
	{
		return;
	}


	RenderCommand::SetRasterizerState(ERasterizerState::SolidFront);
	RenderCommand::BindShader(Shader.get());
	RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	RenderCommand::BindMesh(InOutline.GetMesh());

	FMatrix World = InOutline.GetWorldMatrix().GetTransposed();
	FMatrix ViewProj = InViewProj.GetTransposed();
	FVector4 Scale = InOutline.GetTargetScale();
	FOutlineData OutlineConst = { World, ViewProj, CameraPos, Scale };
	RenderCommand::UpdateBufferData(ConstantBuffer.get(), &OutlineConst, sizeof(OutlineConst));
	RenderCommand::BindConstantBuffer(0, ConstantBuffer.get(), EShaderBindFlagBits::Vertex);
	RenderCommand::DrawIndexed(InOutline.GetMesh()->IndexBuffer->GetIndexCount());
}

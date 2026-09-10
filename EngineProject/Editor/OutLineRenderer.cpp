#include "EnginePCH.h"
#include "OutLineRenderer.h"
#include "Engine/ResourceManager.h"

void FOutlineRenderer::Init(FRenderer* InRenderer)
{
	Renderer = InRenderer;
	Shader = FResourceManager::GetInstance().GetShader("Shader/OutlineShader.hlsl");
	D3D11_RASTERIZER_DESC RasterizerDesc = {};
	RasterizerDesc.FillMode = D3D11_FILL_SOLID;
	RasterizerDesc.CullMode = D3D11_CULL_FRONT;
	RasterizerDesc.FrontCounterClockwise = FALSE;
	Renderer->GetDevice()->CreateRasterizerState(&RasterizerDesc, &RasterizerState);
	ConstantBuffer = Renderer->CreateConstantBuffer(sizeof(FOutlineData));
}

void FOutlineRenderer::SetMesh(FMesh* InMesh)
{
	Mesh = InMesh;
}

void FOutlineRenderer::OnRender(const FOutline& InOutline, const FMatrix& InViewProj, const FVector4& CameraPos) 
{
	if (!InOutline.GetTarget())
	{
		return;
	}


	Renderer->GetDeviceContext()->RSSetState(RasterizerState.Get());
	Renderer->BindShader(Shader);
	Renderer->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	Renderer->BindMesh(InOutline.GetMesh());

	FMatrix World = InOutline.GetWorldMatrix().GetTransposed();
	FMatrix ViewProj = InViewProj.GetTransposed();
	FVector4 Scale = InOutline.GetTargetScale();
	FOutlineData OutlineConst = { World, ViewProj, CameraPos, Scale };
	Renderer->UpdateConstantBufferData(ConstantBuffer.get(), &OutlineConst, sizeof(OutlineConst));
	Renderer->BindConstantBuffer(0, ConstantBuffer.get(), EShaderBindFlagBits::Vertex);
	Renderer->DrawIndexed(InOutline.GetMesh()->IndexBuffer->GetIndexCount());
}

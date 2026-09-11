#include "EnginePCH.h"
#include "PrimitiveComponent.h"
#include "../Render/Renderer.h"
#include "Asset/AssetManager.h"

void UPrimitiveComponent::BeginPlay()
{
	Super::BeginPlay();
	Shader = UAssetManager::Get().GetShader("Shader/DefaultShader.hlsl");
}

void UPrimitiveComponent::SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue)
{
	if (Mesh && Shader)
	{
		FRenderPacket rp;
		rp.mesh = Mesh;
		rp.shader = Shader;
		rp.model = GetWorldMatrix();
		RenderQueue.Enqueue(rp);
	}
}

void UPrimitiveComponent::SetMesh(UStaticMesh* InMesh)
{
	Mesh = InMesh;
}

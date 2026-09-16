#include "EnginePCH.h"
#include "BillboardComponent.h"

#include "Asset/AssetManager.h"


UBillboardComponent::UBillboardComponent()
{
}

UBillboardComponent::~UBillboardComponent()
{
}

void UBillboardComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UBillboardComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}

FMatrix UBillboardComponent::GetBillboardWorldMatrix(const FMatrix& ViewMatrix)
{
	return FMatrix();
}

void UBillboardComponent::SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue)
{
	FRenderPacket rp;
	rp.mesh = Mesh;
	rp.material = Material;
	rp.model = GetWorldMatrix();

	// Todo: subuv
	//rp.bSubUV = false;

	RenderQueue.Enqueue(rp);

	//// Position
	//OutWorldMatrix->M[3][0] = WorldPos.X;
	//OutWorldMatrix->M[3][1] = WorldPos.Y;
	//OutWorldMatrix->M[3][2] = WorldPos.Z;
	//OutWorldMatrix->M[3][3] = 1.0f;
}

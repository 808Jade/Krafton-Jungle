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

void UBillboardComponent::SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue)
{
	FRenderPacket rp;
	rp.mesh = Mesh;
	rp.material = Material;
	rp.model = GetWorldMatrix();

	RenderQueue.Enqueue(rp);
}

void UBillboardComponent::GetWorldTransformedMatrix(FMatrix* OutWorldMatrix) const
{
	OutWorldMatrix->SetIdentity();

	const FTransform& Transform = GetOwner()->GetWorld()->GetMainCamera()->GetCameraComponent()->GetTransform();

	FVector Right = Transform.GetRight().Normalized();
	FVector Up = Transform.GetUp().Normalized();
	FVector Forward = Transform.GetForward().Normalized();

	FVector BbUp = Transform.GetUp().Normalized();
	FVector BbRight = FVector::Cross(BbUp, Forward).Normalized();
	FVector BbFwd = FVector::Cross(BbUp, BbRight);

	if (BbRight.Length() <= 1e-6f)
	{
		BbRight = Right;
		BbFwd = FVector::Cross(BbUp, BbRight);
	}

	const FVector WorldPos = GetWorldLocation();
	const FVector WorldScale = GetWorldScale3D();

	// Y -> Billboard Right
	OutWorldMatrix->M[0][0] = BbFwd.X;
	OutWorldMatrix->M[0][1] = BbFwd.Y;
	OutWorldMatrix->M[0][2] = BbFwd.Z;
	OutWorldMatrix->M[0][3] = 0.0f;

	// Z -> Billboard Up
	OutWorldMatrix->M[1][0] = BbRight.X;
	OutWorldMatrix->M[1][1] = BbRight.Y;
	OutWorldMatrix->M[1][2] = BbRight.Z;
	OutWorldMatrix->M[1][3] = 0.0f;

	// X -> Billboard Forward
	OutWorldMatrix->M[2][0] = BbUp.X;
	OutWorldMatrix->M[2][1] = BbUp.Y;
	OutWorldMatrix->M[2][2] = BbUp.Z;
	OutWorldMatrix->M[2][3] = 0.0f;

	// Position
	OutWorldMatrix->M[3][0] = WorldPos.X;
	OutWorldMatrix->M[3][1] = WorldPos.Y;
	OutWorldMatrix->M[3][2] = WorldPos.Z;
	OutWorldMatrix->M[3][3] = 1.0f;
}

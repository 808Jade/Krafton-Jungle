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
	FVector R = { ViewMatrix.M[0][0], ViewMatrix.M[1][0], ViewMatrix.M[2][0] };
	FVector U = { ViewMatrix.M[0][1], ViewMatrix.M[1][1], ViewMatrix.M[2][1] };
	FVector F = { ViewMatrix.M[0][2], ViewMatrix.M[1][2], ViewMatrix.M[2][2] };

	const FVector S = GetWorldScale3D();
	const FVector P = GetWorldLocation();

	FMatrix M;
	M.M[0][0] = R.X * S.X; M.M[0][1] = R.Y * S.X; M.M[0][2] = R.Z * S.X; M.M[0][3] = 0;
	M.M[1][0] = U.X * S.Y; M.M[1][1] = U.Y * S.Y; M.M[1][2] = U.Z * S.Y; M.M[1][3] = 0;
	M.M[2][0] = F.X * S.Z; M.M[2][1] = F.Y * S.Z; M.M[2][2] = F.Z * S.Z; M.M[2][3] = 0;
	M.M[3][0] = P.X;       M.M[3][1] = P.Y;       M.M[3][2] = P.Z;       M.M[3][3] = 1;

	return M;
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

}

#include "EnginePCH.h"
#include "SkinnedMeshComponent.h"
#include "Render/RenderCommand.h"
#include "UObject/UObjectGlobals.h"

void USkinnedMeshComponent::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
	if (SkeletalMesh == InSkeletalMesh)
		return;

	SkeletalMesh = InSkeletalMesh;
	SkinnedRenderMesh = nullptr;
	SkinnedVertices.Reset();
	ComponentSpace.Reset();
	ClearOverrideMaterials();
	MarkRenderStateDirty();
	OnTransformDirty();
}

UStaticMesh* USkinnedMeshComponent::GetRenderMesh() const
{
	if (SkinnedRenderMesh)
		return SkinnedRenderMesh;
	return SkeletalMesh ? SkeletalMesh->RenderMesh : nullptr;
}

const FStaticMeshData* USkinnedMeshComponent::GetMeshData() const
{
	UStaticMesh* RenderMesh = GetRenderMesh();
	return RenderMesh ? &RenderMesh->GetMeshData() : nullptr;
}

int32 USkinnedMeshComponent::GetNumMaterials() const
{
	UStaticMesh* RenderMesh = GetRenderMesh();
	return RenderMesh ? static_cast<int32>(RenderMesh->GetMeshData().MaterialSlots.Num()) : 0;
}

FString USkinnedMeshComponent::GetMaterialSlotName(int32 SlotIndex) const
{
	if (SlotIndex < 0 || SlotIndex >= GetNumMaterials())
		return FString();
	return GetRenderMesh()->GetMeshData().MaterialSlots[SlotIndex].Name;
}

UMaterial* USkinnedMeshComponent::GetDefaultMaterial(int32 SlotIndex) const
{
	UStaticMesh* RenderMesh = GetRenderMesh();
	return RenderMesh ? RenderMesh->GetMaterial(static_cast<uint32>(SlotIndex)) : nullptr;
}

// 에셋 메시를 복제해 이 컴포넌트 전용 메시(Dynamic VB)를 만든다
bool USkinnedMeshComponent::EnsureSkinnedRenderMesh()
{
	if (SkinnedRenderMesh)
		return true;

	UStaticMesh* Source = SkeletalMesh ? SkeletalMesh->RenderMesh : nullptr;
	if (!Source)
		return false;

	const FStaticMeshData& SourceData = Source->GetMeshData();
	const uint32 VertexBytes = sizeof(FVertex) * static_cast<uint32>(SourceData.Vertices.Num());

	// 1. 정점: 매 갱신마다 덮어쓰므로 Dynamic
	TUniquePtr<FVertexBuffer> VB = RenderCommand::CreateDynamicVertexBuffer(VertexBytes, sizeof(FVertex));
	// 2. 인덱스: 바뀌지 않으므로 Static
	TUniquePtr<FIndexBuffer> IB = RenderCommand::CreateStaticIndexBuffer(SourceData.Indices.GetData(), static_cast<uint32>(SourceData.Indices.Num()));
	if (!VB || !IB)
		return false;

	// 3. 메시 데이터·머티리얼은 에셋에서 복사 (피킹·바운드는 바인드 자세 기준)
	UStaticMesh* Mesh = NewObject<UStaticMesh>();
	Mesh->MeshData = SourceData;
	Mesh->Materials = Source->Materials;
	Mesh->VertexBuffer = std::move(VB);
	Mesh->IndexBuffer = std::move(IB);
	Mesh->RebuildRenderData();

	SkinnedRenderMesh = Mesh;
	SkinnedVertices = SourceData.Vertices;

	// 4. 프록시가 새 메시를 보도록 갱신 요청
	MarkRenderStateDirty();
	return true;
}

void USkinnedMeshComponent::RefreshSkinning(const TArray<FMatrix>& LocalPose)
{
	if (!SkeletalMesh || !SkeletalMesh->RenderMesh || !EnsureSkinnedRenderMesh())
		return;

	const TArray<FBoneInfo>& Bones = SkeletalMesh->Data.Bones;
	const TArray<FSkinWeight>& Weights = SkeletalMesh->Data.SkinWeights;
	const TArray<FVertex>& Source = SkeletalMesh->RenderMesh->GetMeshData().Vertices;
	if (LocalPose.Num() != Bones.Num() || Weights.Num() != Source.Num())
	{
		HTR_LOG(Warning, "Skinning skipped: pose {}, bones {}, weights {}, verts {}", LocalPose.Num(), Bones.Num(), Weights.Num(), Source.Num());
		return;
	}

	// 1. Component Space: 부모부터 순서대로 C = L × C(부모)
	ComponentSpace.SetNum(Bones.Num());
	TArray<FMatrix> SkinMatrices;
	SkinMatrices.SetNum(Bones.Num());
	for (int32 i = 0; i < Bones.Num(); ++i)
	{
		const int32 Parent = Bones[i].ParentIndex;
		ComponentSpace[i] = Parent >= 0 ? LocalPose[i] * ComponentSpace[Parent] : LocalPose[i];

		// 2. Skinning Matrix: 바인드 자세 지우고 지금 자세 입히기
		SkinMatrices[i] = Bones[i].InvBindPose * ComponentSpace[i];
	}

	// 3. 정점마다 뼈 4개의 결과를 비율대로 섞기
	for (int32 v = 0; v < Source.Num(); ++v)
	{
		const FVertex& In = Source[v];
		const FVector InTangent(In.Tangent.X, In.Tangent.Y, In.Tangent.Z);
		FVector Position = FVector::ZeroVector;
		FVector Normal = FVector::ZeroVector;
		FVector Tangent = FVector::ZeroVector;

		for (int32 k = 0; k < 4; ++k)
		{
			const float W = Weights[v].Weights[k];
			const int32 Bone = Weights[v].BoneIndices[k];
			if (W <= 0.0f || Bone < 0 || Bone >= Bones.Num())
				continue;

			const FMatrix& S = SkinMatrices[Bone];
			Position += S.TransformPosition(In.Position) * W;
			Normal += S.TransformVector(In.Normal) * W;
			Tangent += S.TransformVector(InTangent) * W;
		}

		FVertex& Out = SkinnedVertices[v];
		Out.Position = Position;
		Out.Normal = Normal.GetSafeNormal(SMALL_NUMBER, In.Normal);
		const FVector T = Tangent.GetSafeNormal(SMALL_NUMBER, InTangent);
		Out.Tangent = FVector4(T.X, T.Y, T.Z, In.Tangent.W);
	}

	// 4. GPU 업로드 (Map WRITE_DISCARD)
	RenderCommand::UpdateBufferData(SkinnedRenderMesh->VertexBuffer.get(), SkinnedVertices.GetData(),
		sizeof(FVertex) * static_cast<uint32>(SkinnedVertices.Num()));
}

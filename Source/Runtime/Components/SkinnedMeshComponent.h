#pragma once

#include "MeshComponent.h"
#include "Render/SkeletalMesh.h"
#include "SkinnedMeshComponent.generated.h"

UCLASS()
class USkinnedMeshComponent : public UMeshComponent
{
	GENERATED_BODY()
public:
	// 1. 에셋 지정
	void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh);
	USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }

	// 2. 렌더러·피킹용 메시
	virtual UStaticMesh* GetRenderMesh() const override;
	virtual const FStaticMeshData* GetMeshData() const override;

	// 3. 머티리얼 슬롯
	virtual int32 GetNumMaterials() const override;
	virtual FString GetMaterialSlotName(int32 SlotIndex) const override;
	virtual UMaterial* GetDefaultMaterial(int32 SlotIndex) const override;

	// 4. 스키닝: Local 자세 → Component Space → 정점 변형 → GPU 업로드
	void RefreshSkinning(const TArray<FMatrix>& LocalPose);
	const TArray<FMatrix>& GetComponentSpaceTransforms() const { return ComponentSpace; }

protected:
	UPROPERTY(EditAnywhere)
	USkeletalMesh* SkeletalMesh = nullptr;

	// 뼈별 Component Space 행렬
	TArray<FMatrix> ComponentSpace;

private:
	bool EnsureSkinnedRenderMesh();

	// 이 컴포넌트 전용 그리기 메시 (Dynamic Vertex Buffer)
	UStaticMesh* SkinnedRenderMesh = nullptr;
	// 스키닝 결과 정점
	TArray<FVertex> SkinnedVertices;
};

#pragma once

#include "Asset/RenderAsset.h"
#include "Render/SkeletalMeshData.h"

#include "SkeletalMesh.generated.h"

class UStaticMesh;

// FBX에서 구운 스켈레탈 메시 에셋
UCLASS()
class USkeletalMesh : public URenderAsset
{
	GENERATED_BODY()
public:
	// CPU 원본: 바인드 포즈 정점·가중치·뼈 (CPU 스키닝의 입력)
	FSkeletalMeshData Data;

	// 바인드 포즈 GPU 버퍼·머티리얼 (T-Pose 그리기, 스키닝 결과를 쓸 대상)
	// CreateStaticMesh는 인덱스 순서만 바꾸므로 정점 순서는 Data.Mesh.Vertices와 같다 (규칙 1 유지)
	UStaticMesh* RenderMesh = nullptr;
};

#pragma once

#include "Asset/RenderAsset.h"
#include "Render/SkeletalMeshData.h"
#include "SkeletalMesh.generated.h"

class UStaticMesh;

UCLASS()
class USkeletalMesh : public URenderAsset
{
	GENERATED_BODY()
public:
	// 1. 원본 (뼈 + 메시)
	FSkeletalMeshData Data;
	// 2. 그리기용 메시
	UStaticMesh* RenderMesh = nullptr;
};

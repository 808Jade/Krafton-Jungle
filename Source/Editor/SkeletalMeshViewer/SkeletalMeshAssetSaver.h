#pragma once
#include "Core/EngineString.h"

class FSkeletalPreviewTabBase;

// 프리뷰에서 고친 본 포즈를 스켈레탈 메시 애셋(.fbx.bin)에 저장·복원한다. 상태는 없고 탭의 메시와 컴포넌트를 쓴다.
class FSkeletalMeshAssetSaver
{
public:
	explicit FSkeletalMeshAssetSaver(FSkeletalPreviewTabBase& InTab) : Tab(InTab) {}

	// 현재 프리뷰에서 수정한 본 Transform을 RefPose로 확정하고 .bin 파일에 저장
	bool SaveCurrentMesh();
	// 이름을 입력받아 새로운 .fbx.bin 에셋으로 별도 저장 및 에디터 등록
	bool SaveAsNewBin(const FString& NewAssetName);
	// 변형된 .bin을 원본 FBX 초기 포즈로 즉시 복원
	bool RestoreToOriginalFBXPose();

private:
	// 참조(집약): 이 객체를 가진 탭. 탭이 이 객체보다 오래 산다.
	FSkeletalPreviewTabBase& Tab;
};

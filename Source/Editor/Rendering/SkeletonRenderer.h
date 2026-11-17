#pragma once

#include "Containers/Array.h"
#include "Math/Matrix.h"
#include "Render/PipelineState.h"

class UStaticMesh;
class FConstantBuffer;
class FTexture2D;
struct FSkeletalBone;

// 본(뼈대) 한 벌을 그리는 데 필요한 입력. 프리뷰 소유자가 이번 프레임 값으로 채우고 View에 실어 보낸다.
// 렌더 자원은 담지 않는다. 그리는 쪽(FSkeletonRenderer)이 자원을 가진다.
struct FSkeletonDrawParams
{
	// 참조(집약): 메시 애셋의 본 계층. 애셋이 프리뷰보다 오래 산다.
	const TArray<FSkeletalBone>* Bones = nullptr;
	// 본별 월드 행렬. Bones와 개수가 같아야 그린다.
	TArray<FMatrix> BoneWorldMatrices;
	int32 SelectedBone = -1;
	int32 HoveredBone = -1;
	// true면 반투명 팔면체(원뿔), false면 얇은 선
	bool bDrawCones = true;
	float ConeWidthRatio = 0.10f;
	// 관절 구 크기 배율. 메시 반지름에 비례해 실제 크기를 정한다.
	float JointRadius = 2.5f;
	float MeshRadius = 1.0f;
};

// 스켈레탈 메시 프리뷰의 본을 3D 메시(팔면체·구)로 그린다. 엔진이 소유하고 오버레이 패스에서 호출한다.
// (예전에는 스켈레탈 뷰어가 직접 그렸다. Week11처럼 그리기는 엔진 쪽으로 모으고, 뷰어는 무엇을 그릴지만 넘긴다)
class FSkeletonRenderer
{
public:
	FSkeletonRenderer();
	~FSkeletonRenderer();
	FSkeletonRenderer(const FSkeletonRenderer&) = delete;
	FSkeletonRenderer& operator=(const FSkeletonRenderer&) = delete;

	bool Init();
	// 장면 깊이를 지우고 본끼리만 깊이 판정해 메시에 파묻히지 않게 그린다. 오버레이 렌더 패스 안에서 호출한다.
	void Render(const FSkeletonDrawParams& Params, const FMatrix& ViewProjection, FTexture2D* DepthStencil);

private:
	void DrawBone(const FMatrix& World, const FMatrix& ViewProjection, const FVector4& Color, uint32 IndexCount);

	// 참조(집약): 애셋 매니저가 소유하는 메시
	UStaticMesh* BoneMesh = nullptr;
	UStaticMesh* JointMesh = nullptr;
	// 참조(집약): 리소스 매니저가 소유하는 셰이더
	FShaderProgram* Shader = nullptr;
	FPipelineState PipelineState{};
	// 소유(합성): 본 하나마다 갱신하는 상수 버퍼
	TUniquePtr<FConstantBuffer> ConstantBuffer;
};

#pragma once
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Math/Rotator.h"
#include <memory>

class FSkeletalPreviewTabBase;
class FCameraShakePattern;

// 스켈레탈 프리뷰의 카메라와 뷰포트 조작. 공전·자유 이동·팬·줌·포커스, 노티파이가 일으킨 카메라 셰이크를 맡는다.
// 뷰포트 단축키(W/E/R 기즈모 모드, Ctrl+S 저장, L+드래그 조명 회전)도 같은 입력 처리에서 탭으로 넘긴다.
// 최종 카메라(셰이크 반영)는 Cached* 값으로 남겨 렌더(View)와 본 피킹이 같이 쓴다.
class FSkeletalPreviewCamera
{
public:
	explicit FSkeletalPreviewCamera(FSkeletalPreviewTabBase& InTab);
	~FSkeletalPreviewCamera();

	void UpdateCamera(float DeltaTime);
	void HandleInput(float DeltaTime);
	// 선택한 메시(와 보이는 FBX 정적 파츠)가 화면에 꽉 차게 카메라를 옮긴다.
	void FocusOnMesh();

	// 레벨 에디터와 같은 공전 및 자유 이동 카메라 조작을 사용한다.
	FVector CameraLocation = FVector(-250.0f, -250.0f, 150.0f);
	FVector TargetPosition = FVector::ZeroVector;
	float CameraDistance = 250.0f;
	float CameraYaw = 45.0f;
	float CameraPitch = 20.0f;
	float CameraSpeed = 500.0f;
	float FovDegrees = 60.0f;
	FVector CachedCameraLocation = FVector::ZeroVector;
	FRotator CachedCameraRotation;
	FMatrix CachedViewMatrix = FMatrix::Identity;
	FMatrix CachedProjMatrix = FMatrix::Identity;

	// 소유(합성): 진행 중인 카메라 셰이크. 노티파이(카메라 셰이크)가 만들고 끝나면 비운다.
	std::unique_ptr<FCameraShakePattern> PreviewShake;
	float PreviewShakeScale = 1.0f;

private:
	// 참조(집약): 이 카메라를 가진 탭. 메시 크기·뷰포트 크기·Hover 상태를 읽는다. 탭이 이 객체보다 오래 산다.
	FSkeletalPreviewTabBase& Tab;
};

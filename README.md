# Krafton Jungle Game TechLab 4기 - 프로젝트 아카이브
> C++ 및 DirectX 11 기반 게임 엔진 및 렌더링 파이프라인 개발  

크래프톤 정글 게임테크랩 4기 과정 동안 주차별로 설계 및 구현한 작업 내역을 정리한 저장소입니다.

---

## 주차별 수행 내역 (Week 01 ~ Week 09)

### [Week 01] 2D 물리 엔진 및 게임 ('초능력 알까기')
* **개요**: Win32 API 및 DirectX 11 기반 2D 턴제 물리 대전 게임
* **수행 내역**:
  * FMOD 사운드 엔진 연동 (BGM 및 충돌 효과음 재생)
  * 씬 전환 시 페이드 인/아웃 화면 처리 (`UFadeoutOverlay`)
  * 드래프트 씬과 인게임 씬 간 UI 슬롯 데이터 바인딩 및 상태 전이 처리
  * FrameWork 설계
* **기술 스택**: `C++17`, `DirectX 11`, `FMOD Engine`, `Win32 API`

---

### [Week 02] 3D Pepe Engine 기초 렌더러 & 에디터
* **개요**: 3D 렌더러 기초 파이프라인, 기즈모, 카메라 및 에디터 기반 구성
* **수행 내역**:
  * 카메라 거리에 반응하는 적응형 그리드 및 XYZ 월드 축 라인 렌더링
  * `DepthStencilState` 제어를 통한 기즈모 렌더 순서 및 깊이 테스트 처리
  * DirectX 11 디바이스 및 스왑체인 초기화/해제 루틴 리팩토링
  * `FMatrix::TransformPosition` 행렬 연산 유틸 및 평면 피킹 하이라이트 로직 구현
* **기술 스택**: `DirectX 11`, `HLSL`, `DepthStencilState`, `3D Math`

---

### [Week 03] Hitori Engine 에디터 툴 및 경로 추적
* **개요**: 에디터 기능 확장 및 라인 배칭 기반 오브젝트 궤적 추적 도구 개발
* **수행 내역**:
  * 오브젝트 이동 경로 런타임 수집 및 라인 배칭 시각화 시스템 (`PathTracker`) 개발
  * ImGui 한글 폰트 적용 및 경로 추적 제어 패널 구현
  * 오브젝트 선택 외곽선 셰이더(`OutlineShader.hlsl`) 보정 및 UUID 기반 엔티티 관리 적용
* **기술 스택**: `Line Batching`, `ImGui`, `HLSL Post-Outline`, `Entity UUID`

---

### [Week 04] 멀티 뷰포트 아키텍처 리팩토링
* **개요**: 단일 뷰포트 구조를 분리하여 다중 뷰포트(Top/Bottom/Perspective) 지원 및 에디터 구조 개편
* **수행 내역**:
  * `FViewport`(렌더타겟 관리)와 `FViewportClient`(입력 및 카메라 제어) 구조적 분리
  * 뷰포트별 독립 렌더타겟 바인딩 및 HUD 통계 분리 출력
  * 윈도우 리사이즈 예외 처리 및 텍스처 중복 import 시 발생하던 메모리 누수 수정
  * `editor.ini` 직렬화를 통한 뷰포트 레이아웃 저장/복원 및 기즈모 좌표계 전환 오류 수정
* **기술 스택**: `Architecture Refactoring`, `Multiple Viewports`, `State Serialization`

---

### [Week 05] 렌더링 성능 최적화 및 오클루전 컬링(# Brute Force Competition)
* **개요**: 렌더링 병목 해소를 위한 공간 분할, 컬링 알고리즘 및 하드웨어 제어
* **수행 내역**:
  * 오클루전 컬링(Occlusion Culling) 구현을 통한 드로우콜 감소
  * WorldAABB 캐싱 및 거리 기반 LOD 컬링 처리
  * NVAPI 연동을 통한 GPU 클럭 고정(P0) 적용
  * `D3D11_CREATE_DEVICE_SINGLETHREADED` 적용 및 Present 대기 시간 최적화
* **기술 스택**: `Occlusion Culling`, `NVAPI`, `AABB Caching`, `DirectX 11 Flags`

---

### [Week 06] 포스트 프로세싱 및 컴포넌트 시스템
* **개요**: 후처리 파이프라인 구축 및 액터 회전 컴포넌트 시스템 개발
* **수행 내역**:
  * 언리얼 엔진 방식 ACES 톤매핑 및 색상 보정 파이프라인 구현
  * 피벗 기준 회전을 지원하는 `RotatingMovementComponent` 구현
  * 블린-퐁(Blinn-Phong) 라이팅 보정 및 Rotator 프로퍼티 직렬화 연동
  * 포스트 프로세싱 검증용 테스트 씬('Murasaki') 구성
* **기술 스택**: `Post-Processing`, `ACES Tone Mapping`, `Blinn-Phong`, `Component System`

---

### [Week 07] 조명 연산 및 노멀 매핑
* **개요**: 노멀 매핑 파이프라인 디버깅 및 에디터 보조 시각화 기능 추가
* **수행 내역**:
  * 노멀 매핑 시 탄젠트/바이노멀 계산 오류 및 특정 각도 빛 번짐 현상 수정
  * 조명 위치 표시용 빌보드 아이콘 렌더링 및 방향 벡터 화살표 라인 배칭 구현
  * PIE(Play-In-Editor) 모드 시 에디터 기즈모 및 빌보드 자동 은닉 처리
  * Directional Light 검증용 시네마틱 씬('ItsHighNoon') 제작
* **기술 스택**: `Normal Mapping (Tangent Space)`, `Light Billboards`, `Line Batching`, `PIE State`

---

### [Week 08] Directional Light CSM (Cascaded Shadow Maps)
* **개요**: 4단 CSM 구축 및 그림자 아티팩트 해결
* **수행 내역**:
  * 시야 절두체 분할 기반 4단 Cascaded Shadow Maps 파이프라인 구축
  * 절두체 외접구(Bounding Sphere) 투영 및 텍셀 스내핑(Texel Snapping)을 적용해 카메라 회전 시 그림자 외곽선 떨림(Shimmering) 해결
  * 하드웨어 깊이 클램핑(`DepthClipEnable = FALSE`) 및 셰이더 클램핑(Shadow Map Pancaking)을 적용해 시야 밖 캐스터로 인한 그림자 잘림 방지
  * 캐스케이드 간 해상도 차이를 보정하는 Cascade Blending (`lerp`) 구현
  * CSM 원리 검증을 위한 웹 기반 인터랙티브 시뮬레이터 제작
* **기술 스택**: `CSM`, `Texel Snapping`, `Depth Clamping (Pancaking)`, `Cascade Blending`

---

### [Week 09] 실시간 소프트 섀도우 (PCF/PCSS) 및 시스템 통합
* **개요**: 물리 기반 가변 연초점 그림자 구현 및 렌더링 파이프라인 통합
* **수행 내역**:
  * 차폐체 거리 기반 PCF 및 PCSS(Percentage-Closer Soft Shadows) 필터링 구현
  * 정적 오브젝트 대상 섀도우 맵 캐싱(Shadow Map Caching) 적용
  * 포인트 라이트 큐브맵 섀도우 맵 경계선 아티팩트 수정
  * 셰이더 컴파일 타임 최적화 및 섀도우 바이어스 튜닝 완료
* **기술 스택**: `PCF`, `PCSS`, `Shadow Map Caching`, `CubeMap Shadow`, `Shader Optimization`

---

## 기술 스택 요약

* **Language**: C++17, HLSL
* **Graphics API**: DirectX 11, DirectX 12, Win32 API
* **Rendering**: Cascaded Shadow Maps (CSM), PCF/PCSS, Normal Mapping, ACES Tone Mapping, Depth Clamping, Texel Snapping
* **Optimization**: Occlusion Culling, Tile-Based Light Culling, NVAPI GPU Clock Locking, LOD, AABB Caching
* **Tools & UI**: Multi-Viewport, ImGui, Line Batching, Path Tracker, Scene Serializer

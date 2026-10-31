# ADR-0001: FBX Importer 이식 — 에디터가 굽고, 런타임은 구조체(.bin)만 읽는다

**Status:** Accepted
**Date:** 2026-10-30
**Deciders:** 경호
**참고:** [UE FBX 임포트 조사](../Research/UE-FBX-Import.md)

## Context

- Week 10 목표: FBX SDK를 엔진에 통합하고, FBX 자료 구조를 이해한다. UE의 방식을 모방해 **왜 UE가 그렇게 했는지** 이해하는 것이 목적이다.
- 현재 OBJ 경로: `UAssetManager::ScanAssets` → `LoadAsset(".obj")` → `FObjImporter::LoadStaticMeshData`(`.obj.bin` 캐시 읽기 → 없으면 Parse·Cook → `.bin` 저장) → `CreateStaticMesh`. 이 경로는 전부 Runtime(`HitoriEngine`)에 있다.
- 같은 자리(Runtime)에 FBX Importer를 두면 `HitoriGame`(패키징 빌드)도 FBX SDK를 링크해야 하고, 게임이 원본 포맷과 그 라이브러리를 알게 된다.
- UE는 FBX를 읽는 코드를 에디터 모듈(`UnrealEd`)에만 두고, 런타임은 구워진 에셋만 읽는다.
- 확인한 사실
  - `FEngineLoop::PreInit`은 `GEngine`(에디터 엔진 객체)을 **먼저** 만들고 그다음 `UAssetManager::Init`(스캔)을 부른다 → 에디터가 스캔 전에 끼어들 수 있다.
  - `FStaticMeshData`의 후처리(`BuildTangentSpace`, `OptimizeTriangleOrderForVertexCache`)는 정점 배열의 순서·개수를 바꾸지 않는다 → 정점과 짝을 이루는 별도 배열이 안전하다.
  - 우리 `TDelegate`는 반환값 없는 멀티캐스트(UE의 `TMulticastDelegate`에 해당)다 → "굽는 사람" 자리에는 맞지 않는다. UE도 반환값은 단일 delegate에만 허용한다(`DelegateCombinations.h`: `DECLARE_DELEGATE_RetVal_*`는 단일, `DECLARE_MULTICAST_DELEGATE_*`는 모두 `void`). `Delegate.h`에 단일 delegate를 추가하는 안도 검토했으나, 하는 일이 같으므로(호출 대상 하나 + 반환값) `std::function`을 그대로 쓴다(기존 `FAssetLoadProgress`와 같은 방식).
  - UE에서 런타임 코드가 에디터 기능을 부르는 방식: `USkeletalMesh::BuildLODModel`(Runtime/Engine, `#if WITH_EDITOR` 안)이 `IMeshBuilderModule::GetForPlatform(...).BuildSkeletalMesh`를 호출한다. 인터페이스는 `Developer/MeshBuilder`, 구현은 모듈 매니저가 찾아오고, 게임 빌드에서는 컴파일에서 빠진다. 우리 엔진은 모듈 매니저와 `WITH_EDITOR` 분리가 없으므로(에디터·게임이 `HitoriEngine` 하나를 공유) "런타임이 빈 자리를 선언하고 에디터가 채운다"로 같은 효과를 낸다.
  - `FMatrix`는 행벡터 규약(`// Row Vector sys`), UE와 같다.
  - 패키징은 에디터 안에서 `Package.bat`을 실행한다(`GamePackager.cpp`) → 패키징 시점에는 에디터가 이미 구운 상태다.

## Decision

**FBX SDK를 쓰는 Importer는 에디터에만 둔다. 런타임과는 `FSkeletalMeshData` 구조체와 그 `.bin` 형식으로만 주고받는다.**
AssetManager의 FBX 경로는 OBJ와 같은 모양(캐시 읽기 → 없으면 굽기 → 저장 → 등록)이고, "굽기" 자리만 비어 있어 에디터가 채운다.

### 결정 사항 (A단계)

| # | 결정 |
|---|---|
| A-1 | `FSkeletalMeshData` = `Mesh`(기존 `FStaticMeshData`) + `SkinWeights`(정점별 `int32 BoneIndex[12]`, `float Weight[12]` — `MAX_TOTAL_INFLUENCES = 12`, UE 임포트 상한과 같음. 안 쓰는 칸은 Weight 0) + `Bones`(`Name`, `ParentIndex`, `InverseBindPose`) |
| A-2 | `SkinWeights[i]` ↔ `Mesh.Vertices[i]` 짝(순서·개수 동일) / `Bones`는 부모가 자식보다 먼저 |
| A-3 | 굽는 사람 자리 = `std::function<bool(const FString& FbxPath, FSkeletalMeshData& OutData)>` 하나(`UAssetManager::SetSkeletalMeshImporter`). 에디터가 `UEngine`의 새 virtual에서 **스캔 전에** 등록. 게임에서는 비어 있음 |
| A-4 | 에셋 키 = `.fbx` 상대 경로, 데이터 = `X.fbx.bin`. 최신성 검사(`.fbx`가 더 최신이면 다시 굽기)는 굽는 사람이 있을 때만 |
| A-5 | 정점 = 모델 공간(바인드 순간). ControlPoint에 메시 노드의 바인드 시점 전역 행렬(`FbxCluster::GetTransformMatrix`)을 적용 |
| A-6 | 행벡터 규약: `v' = v · InverseBindPose · BoneTransform`. FBX 행렬은 원소를 그대로 복사. 축 변환은 `ConvertScene` 대신 **파일 공간으로 다 읽은 뒤 행렬 C 하나로** 정점·노멀·바인드 행렬을 함께 바꾼다(Y-Up: OBJ `ToEngine`과 같은 규칙, Z-Up: Y 반전). 행렬은 `Cᵀ·F·C` |
| A-7 | 단위 변환 없음, 축만 변환(OBJ와 동일: 파일 숫자 = 월드 숫자) |
| A-8 | 바인드 행렬: 모든 뼈를 0프레임 전역 행렬로 채운 뒤, Cluster가 있는 뼈는 `GetTransformLinkMatrix`로 덮어쓴다(Cluster 없는 루트·`_End` 뼈 대비) |
| A-9 | (ControlPoint 번호, 노멀, UV, 머티리얼)이 같으면 같은 정점 |
| A-10 | 텍스처 = `FbxFileTexture::GetRelativeFileName`(`.fbx` 기준) → 저장소 루트 기준 `Assets/...`로 저장. 없으면 기본 텍스처 + 경고(이후 사용자가 직접 배치) |

### 제외 (나중에 확장해도 구조가 바뀌지 않음)

- 임포트 버튼·드래그 앤 드롭(에디터 실행 중 임포트) — 굽는 함수는 같고 호출 시점만 늘어난다.
- 애니메이션(AnimStack) 추출 — 애니메이션 주제에서 별도 에셋으로.
- 패키지에서 `.fbx` 원본 제외 — 먼저 "`.fbx.bin`만 있어도 `.fbx` 키로 등록" 규칙이 필요하다.

## Options Considered

### Option A: Runtime Importer (OBJ와 같은 자리)
| Dimension | Assessment |
|---|---|
| Complexity | Low — 기존 OBJ 경로에 분기만 추가 |
| Cost | `HitoriGame`까지 FBX SDK 링크, 게임이 원본 포맷에 의존 |
| Scalability | Importer를 나중에 에디터로 옮길 때 경계가 없으면 SDK 타입이 퍼져 있음 |
| Team familiarity | High |

**Pros:** 가장 적게 바꾼다.
**Cons:** UE 구조(에디터만 원본을 안다)와 다르다. 게임 빌드가 SDK에 묶인다.

### Option B: Editor Importer + Runtime 구조체/.bin (채택)
| Dimension | Assessment |
|---|---|
| Complexity | Med — `UEngine` virtual 1개, 굽는 사람 자리 1개 추가 |
| Cost | `HitoriEditor`만 SDK 링크 |
| Scalability | 임포트 버튼·다른 포맷 Importer를 같은 자리에 끼울 수 있다 |
| Team familiarity | OBJ의 `.bin` 캐시 패턴과 같은 모양 |

**Pros:** 에디터·게임이 **같은 읽기 경로(.bin)** 를 쓴다 → 직렬화가 매 실행 검증되고 "에디터에선 되는데 게임에선 깨짐"이 없다. UE의 분리 이유를 그대로 체험한다.
**Cons:** `.bin` 형식을 처음부터 유지해야 한다(구조 변경 시 version 증가).

### Option C: 에디터 메모리 전용(Bake 없이) 후 나중에 Bake 추가
**Pros:** 처음 범위가 작다.
**Cons:** 에디터(메모리)와 게임(.bin)의 읽기 경로가 두 갈래가 된다. Bake는 선택 과제지만 OBJ에 틀이 이미 있어 추가 비용이 작다 → 통합(Option B)이 낫다.

## Trade-off Analysis

Option A는 코드는 가장 적지만, 이번 주의 목적("UE가 왜 Importer를 에디터에 두었나")을 체험할 수 없고 게임 빌드가 SDK에 묶인다. Option C는 단계는 작지만 읽기 경로가 갈라진다. Option B는 virtual 1개와 함수 자리 1개만 늘리고, 나머지는 OBJ 패턴을 재사용해 경계를 처음부터 올바르게 세운다.

## Consequences

- 쉬워지는 것: 게임은 FBX SDK 없이 빌드된다. 임포트 버튼·재임포트를 같은 굽기 함수로 붙일 수 있다. FBX 안의 StaticMesh 임포트(선택 과제)가 D단계에서 함께 끝난다.
- 어려워지는 것: `FSkeletalMeshData`나 `.bin` 구조를 바꿀 때마다 Bake version을 올려야 한다.
- 다시 볼 것: `.fbx` 원본 패키지 제외, 애니메이션 에셋 분리, `LaunchEngineLoop.cpp`(Runtime) 수정이 다른 엔진 모드에 영향을 주지 않는지.

## Action Items — 로드맵 (생각의 흐름 순서)

> 원칙: 기존 구조(AssetManager)에서 출발해 "다음에 무엇이 필요한가"를 따라간다. 눈에 보이는 것부터, 안 보이는 것으로. 단계마다 눈으로 확인할 결과가 하나씩 있다.

### B. 입구 — AssetManager
- [ ] B-1 `Runtime/Render/SkeletalMeshData.h`: A-1 구조체 정의 + A-2·A-5·A-6 규칙을 주석으로
- [ ] B-2 `LoadAsset`에 `.fbx` 분기 → `LoadFbxMesh(Path)` (OBJ의 `LoadObjStaticMesh`처럼 포맷 이름)
- [ ] B-3 OBJ와 같은 두 층: 위층 `LoadFbxMesh`(키 확인 → 데이터 얻기 → 등록, 지금은 로그) / 아래층 `LoadSkeletalMeshData(Path, Importer)`(최신성 → 캐시 읽기 → 굽는 사람 없으면 경고 → 굽기 → 저장). 실패 로그는 Importer가 남긴다
- [ ] B-4 Bake 헬퍼를 공용 헤더로 이동 + `FSkeletalMeshBake` 껍데기(헤더 magic/version만)
- [ ] B-5 굽는 사람 자리(`std::function` + `SetSkeletalMeshImporter`) + `UEngine::RegisterAssetImporters()`를 `UAssetManager::Init` 직전에 호출 + 에디터 override(가짜: 로그 후 false)
- 상세: [B단계 구현 가이드](0001-fbx-importer-B.md)
- **확인:** 에디터 "importer not implemented yet" 경고 / 게임 "no baked data" 경고 / 기존 OBJ `.bin` 정상 로드 / 게임 빌드에 SDK 없음

### C. 굽는 사람 — `FFbxImporter` (Editor, `fbxsdk.h`를 include하는 유일한 .cpp)
- [ ] C-1 premake: `HitoriEditor`에만 SDK(include, Debug/Release lib 경로, `-md` lib 3종)
- [ ] C-2 열기·정리: Manager → IOSettings → Importer → Scene → Destroy
- [ ] C-3 노드 트리 로그(이름 + 속성 종류) + Cluster 없는 뼈 / 텍스처 내장 여부(`.fbm`) / 노드 Scale 확인
- 상세: [C단계 구현 가이드](0001-fbx-importer-C.md)
- **확인:** 샘플의 Mesh·Skeleton 노드가 트리로 찍힘, Skin 존재

### D. 보이는 것 — 메시
- 상세: [D~H단계 구현 가이드](0001-fbx-importer-D-H.md)
- [ ] D-1 축 변환: 축 변환 행렬 C(A-6) + 감김 반전(A-7: 단위는 그대로)
- [ ] D-2 삼각형화(`FbxGeometryConverter::Triangulate`)
- [ ] D-3 정점 펼치기(A-9 키, A-5 모델 공간) + **정점 → ControlPoint 맵 보존**(F에서 사용)
- [ ] D-4 머티리얼·섹션, 텍스처 경로(A-10)
- [ ] D-5 Bake에 메시 본문(Static과 공유)
- [ ] D-6 뼈가 아직 없으므로 `CreateStaticMesh`로 `UStaticMesh` 등록
- **확인:** 캐릭터가 바른 방향·크기·텍스처로 T-Pose 표시 (= 선택 과제 StaticMesh Import 완료)

### E. 뼈대
- [ ] E-1 Skeleton 노드를 부모 먼저 순서로 `Bones`에, 이름 → 인덱스 맵
- [ ] E-2 Bake에 `Bones` + version 증가
- **확인:** 뼈 계층 들여쓰기 로그(Bone Hierarchy Tree UI의 원본)

### F. 접착 — 가중치
- [ ] F-1 Cluster마다 (ControlPoint, 뼈 인덱스, 비율) 수집(E의 맵 사용)
- [ ] F-2 D-3 맵으로 정점별 복사 → 큰 순 최대 12개(`MAX_TOTAL_INFLUENCES`) → 재정규화
- [ ] F-3 Bake에 `SkinWeights`
- **확인:** `Validate`(개수 일치·인덱스 범위·합≈1), 팔꿈치 근처 정점이 두 뼈에 나뉨

### G. 붙인 순간 — 바인드 포즈
- [ ] G-1 A-8 규칙으로 뼈별 전역 바인드 행렬 → 엔진 `FMatrix`(A-6 축 반전)
- [ ] G-2 역행렬 → `InverseBindPose`
- **확인:** 뼈 위치가 메시 범위 안에 있는가 (정점과 뼈가 같은 공간인지). ※ 이전에 적은 `v·InvBind·Bind == v`는 항상 참이라 검사가 되지 않아 정정

### H. 마무리
- [ ] H-1 `Bones`가 있으면 `USkeletalMesh`, 없으면 `UStaticMesh`로 등록(UE도 Skin 유무로 분기). `LoadFbxMesh` 반환형을 `URenderAsset*`로 넓힘
- [ ] H-2 저장 직후 다시 읽어 개수 비교하는 self-check

### 이후 (이 ADR 범위 밖)
- `USkinnedMeshComponent` / `USkeletalMeshComponent`, CPU Skinning, ShowFlag, SkeletalMesh Viewer(Bone Hierarchy Tree, Gizmo)

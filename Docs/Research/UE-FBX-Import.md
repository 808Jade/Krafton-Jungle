# Unreal Engine FBX 스켈레탈 메시 임포트 파이프라인 — 소스 코드 1차 조사

> 조사 대상: 로컬 UE 소스 `C:\Users\JUNGLE\Desktop\UnrealEngine-release`
> 엔진 버전: **UE 5.8.2** (`Engine/Build/Build.version`: `MajorVersion 5, MinorVersion 8, PatchVersion 2, BranchName "UE5"`)
> 조사 범위: **레거시 FBX 임포터(UnFbx::FFbxImporter)** 경로를 중심으로, `.fbx` 파일 선택부터 `USkeletalMesh` 완성까지.
> 표기 규칙: 모든 사실 주장 뒤에 `(파일, 함수/구조체, 줄 번호)`를 단다. 줄 번호는 이 소스 트리 기준이다. 코드로 확인하지 못한 것은 **미확인**, 코드에 근거가 없는 내 해석은 **추론**이라고 적는다.

---

## 0. 출처 약어표

본문에서는 파일 이름만 쓴다. 실제 경로(UE 루트 기준)는 아래와 같다.

| 본문 표기 | UE 루트 기준 경로 |
|---|---|
| `FbxFactory.cpp` | `Engine/Source/Editor/UnrealEd/Private/Fbx/FbxFactory.cpp` |
| `FbxMainImport.cpp` | `Engine/Source/Editor/UnrealEd/Private/Fbx/FbxMainImport.cpp` |
| `FbxSkeletalMeshImport.cpp` | `Engine/Source/Editor/UnrealEd/Private/Fbx/FbxSkeletalMeshImport.cpp` |
| `FbxUtilsImport.cpp` | `Engine/Source/Editor/UnrealEd/Private/Fbx/FbxUtilsImport.cpp` |
| `FbxImporter.h` | `Engine/Source/Editor/UnrealEd/Public/FbxImporter.h` |
| `SkeletalMeshImportUtils.cpp` | `Engine/Source/Editor/UnrealEd/Private/ImportUtils/SkeletalMeshImportUtils.cpp` |
| `UnrealEd.Build.cs` | `Engine/Source/Editor/UnrealEd/UnrealEd.Build.cs` |
| `SkeletalMeshLODImporterData.h` | `Engine/Source/Runtime/Engine/Public/Rendering/SkeletalMeshLODImporterData.h` |
| `SkeletalMeshLODImporterData.cpp` | `Engine/Source/Runtime/Engine/Private/Rendering/SkeletalMeshLODImporterData.cpp` |
| `SkeletalMesh.cpp` | `Engine/Source/Runtime/Engine/Private/SkeletalMesh.cpp` |
| `SkeletalMesh.h` | `Engine/Source/Runtime/Engine/Classes/Engine/SkeletalMesh.h` |
| `GPUSkinPublicDefs.h` | `Engine/Source/Runtime/Engine/Public/GPUSkinPublicDefs.h` |
| `LODUtilities.cpp` | `Engine/Source/Developer/SkeletalMeshUtilitiesCommon/Private/LODUtilities.cpp` |
| `MeshBuilderModule.cpp` | `Engine/Source/Developer/MeshBuilder/Private/MeshBuilderModule.cpp` |
| `SkeletalMeshBuilder.cpp` | `Engine/Source/Developer/MeshBuilder/Private/SkeletalMeshBuilder.cpp` |
| `MeshUtilities.cpp` | `Engine/Source/Developer/MeshUtilities/Private/MeshUtilities.cpp` |
| `SkeletalMeshTools.cpp` | `Engine/Source/Developer/MeshUtilities/Private/SkeletalMeshTools.cpp` |
| `InterchangeFbxParser.Build.cs` | `Engine/Plugins/Interchange/Runtime/Source/Parsers/Fbx/InterchangeFbxParser.Build.cs` |
| `BaseEngine.ini` | `Engine/Config/BaseEngine.ini` |

---

## 1. 한눈에 보기

### 1-A. 먼저 알아둘 용어

| 용어 | 뜻 (쉬운 말) |
|---|---|
| **FBX SDK** | Autodesk가 배포하는 FBX 파일 읽기/쓰기 C++ 라이브러리. 이 트리에는 2020.2 버전이 들어 있다 (`Engine/Source/ThirdParty/FBX/2020.2` 폴더). |
| **FbxScene / FbxNode** | FBX SDK가 파일을 읽어서 메모리에 만든 "장면 트리". 노드 하나가 메시·뼈·카메라 등 하나를 가리킨다. |
| **Control Point (컨트롤 포인트)** | FBX 메시의 "위치만 있는 꼭짓점". 정육면체라면 8개. UV·노멀은 여기 없다. |
| **Polygon Vertex / Wedge (웨지)** | 삼각형의 "모서리 한 개". 같은 위치(컨트롤 포인트)라도 면마다 UV·색이 다를 수 있어서, 면의 각 모서리를 따로 기록한 것. UE 헤더는 이를 "Hoppe의 'Wedge' 개념과 비슷한, 텍스처 정보를 가진 꼭짓점"이라 설명한다 (`SkeletalMeshLODImporterData.h`, `SkeletalMeshImportData::FVertex` 주석, L224). |
| **Skin / Cluster (스킨 / 클러스터)** | FBX에서 "메시가 뼈에 붙어 있는 방식"을 담는 객체. **클러스터 1개 = 뼈 1개**이고, 그 뼈가 움직이게 하는 컨트롤 포인트 번호 목록과 가중치 목록을 갖는다 (사용처: `FbxSkeletalMeshImport.cpp`, `FillSkelMeshImporterFromFbx`, L3950–3966). |
| **Influence (인플루언스) / Weight (가중치)** | "꼭짓점 v는 뼈 b의 움직임을 w만큼 따른다"는 한 쌍(뼈, 가중치). 스키닝 식은 $v' = \sum_k w_k \, M_{b_k} B_{b_k}^{-1} v$ 이고 보통 $\sum_k w_k = 1$. |
| **Bind Pose (바인드 포즈) / Reference Pose** | 아티스트가 메시를 뼈에 "붙였던 순간"의 뼈 자세. 위 식의 $B_b$(뼈 b의 바인드 시점 전역 행렬). |
| **FbxPose** | FBX 파일 안에 저장된 "자세 스냅샷". 그중 `IsBindPose()`가 참인 것이 바인드 포즈다. |
| **TransformLinkMatrix** | 클러스터가 기억하는 "바인드 시점의 뼈(링크) 전역 행렬". FbxPose가 없을 때의 대체 출처. |
| **FSkeletalMeshImportData** | UE가 FBX에서 꺼낸 데이터를 옮겨 담는 **엔진 쪽 중간 구조체** (`SkeletalMeshLODImporterData.h`, L327). |
| **FMeshDescription** | UE의 범용 "편집용 메시 원본" 형식. 5.8에서는 임포트 데이터가 최종적으로 이 형식으로 에셋에 저장된다 (질문 2 참조). |
| **FSkeletalMeshLODModel** | 빌드 단계가 만든 "에디터용 완성 메시" (섹션·소프트 스킨 버텍스·인덱스 버퍼). |
| **Render Data (FSkeletalMeshRenderData)** | GPU 버퍼로 올리기 위해 플랫폼별로 만든 최종 데이터. |

### 1-B. 흐름 한 장 (요약)

```
[사용자 .fbx 선택]
   │
   ▼
UFbxFactory::FactoryCreateFile ── 옵션 수집, FFbxImporter 싱글턴 획득
   │
   ▼
FFbxImporter::ImportFromFile ── OpenFile → ImportFile(FBX SDK로 FbxScene 생성) → ConvertScene(축·단위 변환)
   │
   ▼
FFbxImporter::ImportSkeletalMesh
   ├─ FillSkeletalMeshImportData ── FBX → FSkeletalMeshImportData (뼈/점/웨지/면/인플루언스)
   ├─ ProcessImportMeshSkeleton / ProcessImportMeshInfluences ── RefSkeleton 생성, 가중치 정리(≤12)
   ├─ SaveLODImportedData ── 중간 데이터를 FMeshDescription으로 에셋에 저장
   └─ IMeshBuilderModule::BuildSkeletalMesh
          └─ FSkeletalMeshBuilder::Build ── MeshDescription → ImportData 복원 → FMeshUtilities::BuildSkeletalMesh
                 → 탄젠트 계산, 인플루언스 제한(8/12), 버텍스 병합, 본 수 기준 청크 분할 → FSkeletalMeshLODModel
   │
   ▼
CalculateInvRefMatrices, USkeleton 생성/병합 → (PostEditChange/Build) → Render Data
```

### 1-C. 질문별 핵심 답 표

| 질문 | 한 줄 답 |
|---|---|
| Q1 흐름 | Factory → FFbxImporter(SDK 로드·ConvertScene) → FillSkeletalMeshImportData → 정리 → MeshDescription 저장 → SkeletalMeshBuilder/MeshUtilities 빌드 → LODModel → RenderData |
| Q2 왜 중간 구조 | 저장된 원본(MeshDescription)만으로 **FBX 없이 재빌드**하고, **지오메트리만/스키닝만 재임포트**하며, 빌드(탄젠트·버텍스 분할·인플루언스 제한·청크)가 **플랫폼·빌드 설정에 따라 달라지기** 때문 (코드 근거). FBX SDK 타입이 빌드 코드에 없다는 점(디커플링)도 코드로 확인됨. |
| Q3 중간 구조 | Points(위치) / Wedges(모서리: 점 번호+UV+색) / Faces(삼각형: 웨지 3개+재질+모서리별 노멀·탄젠트) / Influences(가중치·점·뼈 3개짜리 평평한 목록) / RefBonesBinary(뼈 이름·부모·바인드 로컬 트랜스폼) / PointToRawMap 등. 가중치는 **클러스터(뼈) 순서로 쌓였다가 점 번호로 정렬**된다. |
| Q4 좌표/단위 | ImportFromFile 안의 `ConvertScene`에서 FBX SDK로 **Z-up·오른손 좌표계**로 변환 + **cm** 단위 변환, 그다음 데이터를 꺼낼 때마다 `FFbxDataConverter`가 **Y축 부호를 뒤집어 왼손 좌표계**로 만든다. |
| Q5 Legacy vs Interchange | 레거시(UnrealEd)는 Autodesk FBX SDK 2020.2를 직접 링크. Interchange의 `InterchangeFbxParser` 모듈은 **FBX SDK와 ufbx 0.21.2를 둘 다** 링크한다. 실행 시 기본으로 어느 쪽을 쓰는지는 **미확인**. |

---

## 2. 질문 1 — 호출 흐름 한 장

형식: `단계 — 클래스::함수 (파일, 줄) — 하는 일`

### 단계 0. 진입점 선택 (레거시냐 Interchange냐)

- 0 — `UFbxFactory::GetFbxFormats` (`FbxFactory.cpp`, L1201–1233) — 콘솔 변수 `Interchange.FeatureFlags.Import.FBX`와 `Interchange.FeatureFlags.Import.FBX.ToLevel` 중 **하나라도 꺼져 있을 때만** 레거시 팩토리가 `fbx` 포맷을 등록한다 (`bUseLegacyFbx`, L1206). 즉 둘 다 켜져 있으면 `.fbx`는 Interchange로 간다. 두 CVar의 기본값은 정의 파일을 열지 못해 **미확인**.

이하 1~10단계는 **레거시 경로**다.

### 단계 1. 팩토리

- 1 — `UFbxFactory::FactoryCreateFile` (`FbxFactory.cpp`, L166) — 임포트 진입점. `UnFbx::FFbxImporter::GetInstance()`로 임포터 싱글턴을 얻고(L297), 옵션 창/옵션 값을 모은다(`GetImportOptions`, L370).

### 단계 2. FBX 파일을 FBX SDK 장면으로 읽기

- 2 — `FFbxImporter::ImportFromFile` (`FbxMainImport.cpp`, L1612) — 아래 2-1~2-3을 `switch`의 fall-through로 순서대로 실행.
  - 2-0 — `FFbxImporter::FFbxImporter` (`FbxMainImport.cpp`, L574) — 생성 시 `FbxManager::Create()`(L585)와 `FbxGeometryConverter`(L592)를 만든다. 이것이 FBX SDK 사용의 출발점.
  - 2-1 — `FFbxImporter::OpenFile` (`FbxMainImport.cpp`, L1081) — `FbxImporter::Create`(L1101), `Importer->Initialize(파일명)`(L1113). Blender가 만든 파일인지 헤더로 판별(L1120–1127 부근).
  - 2-2 — `FFbxImporter::ImportFile` (`FbxMainImport.cpp`, L1424) — `FbxScene::Create`, 재질·스킨·디포메이션·애니 등 IO 옵션을 켜고 `Importer->Import(Scene)`(L1458)으로 **FBX 파일 → FbxScene**.
  - 2-3 — `FFbxImporter::ConvertScene` (`FbxMainImport.cpp`, L1499; 호출 L1641) — **축 변환과 단위 변환**(질문 4). OBJ가 아닐 때만 호출.

### 단계 3. 스켈레탈 메시 노드 찾기

- 3 — `FFbxImporter::FillFbxSkelMeshArrayInScene` (호출: `FbxFactory.cpp`, L446) — 장면 트리에서 스켈레탈 메시로 가져올 노드 묶음을 고른다. (함수 본문은 이번에 읽지 않음 → 세부 동작 **미확인**)

### 단계 4. 스켈레탈 메시 임포트 본체

- 4 — `FFbxImporter::ImportSkeletalMesh` (`FbxSkeletalMeshImport.cpp`, L1756; 호출 `FbxFactory.cpp`, L750) — 새 에셋이면 `NewObject<USkeletalMesh>`(L1823). 이후 5~9단계를 진행.

### 단계 5. FBX → 중간 구조(FSkeletalMeshImportData) 채우기

- 5 — `FFbxImporter::FillSkeletalMeshImportData` (`FbxSkeletalMeshImport.cpp`, L1425; 호출 L1853)
  - 5-1 — `FFbxImporter::ImportBones` (L1044) — 클러스터 수집 → 바인드 포즈 확보 → 뼈 정렬(`BuildSkeletonSystem`, L918) → 뼈마다 **바인드 포즈 로컬 트랜스폼**을 계산해 `ImportData.RefBonesBinary`에 넣음 (질문 3).
  - 5-2 — 재질 목록 1차 수집 (L1469–1508) — 삼각형에 재질 번호를 붙이기 전에 전체 재질을 먼저 모은다(주석 L1469–1470).
  - 5-3 — `FFbxImporter::FillSkelMeshImporterFromFbx` (L3256; 호출 L1523) — 메시 노드마다:
    - 불량 폴리곤 제거 `Mesh->RemoveBadPolygons()` (L3261), 뼈 수 상한 `MAX_BONES` 검사 (L3272)
    - 삼각형이 아니면 `GeometryConverter->Triangulate` (L3462)
    - `FillSkeletalMeshImportPoints` (L1582; 호출 L3644) — 컨트롤 포인트 → `Points`
    - 삼각형 루프 (L3663–3933) — 면마다 노멀/탄젠트/재질/UV/색 → `Faces`, `Wedges`
    - 클러스터 루프 (L3936–3969) — `Influences` 생성
  - 5-4 — 스무딩 그룹 처리 (L1553–1566) — `bPreserveSmoothingGroups`면 `DoUnSmoothVerts`(L643)로 스무딩 그룹이 다른 면 경계에서 점을 쪼개고, 아니면 `PointToRawMap[i]=i`.

### 단계 6. 중간 데이터 정리

- 6-1 — `SkeletalMeshImportUtils::ProcessImportMeshMaterials` (호출 `FbxSkeletalMeshImport.cpp`, L2007) — 재질 슬롯 구성.
- 6-2 — `SkeletalMeshImportUtils::ProcessImportMeshSkeleton` (`SkeletalMeshImportUtils.cpp`, L165; 호출 L2012) — `RefBonesBinary` → 엔진의 `FReferenceSkeleton`(이름·부모·트랜스폼). 중복 뼈 이름이면 실패.
- 6-3 — `SkeletalMeshImportUtils::ProcessImportMeshInfluences` (`SkeletalMeshImportUtils.cpp`, L223; 호출 L2025) → `FLODUtilities::ProcessImportMeshInfluences` (`LODUtilities.cpp`, L62) — 가중치 정렬·정규화·12개 제한 (질문 3).

### 단계 7. 중간 데이터를 에셋에 "원본"으로 저장

- 7 — `USkeletalMesh::SaveLODImportedData` (`SkeletalMesh.cpp`, L3254; 호출 `FbxSkeletalMeshImport.cpp`, L2038) — `FSkeletalMeshImportData::GetMeshDescription`(`SkeletalMeshLODImporterData.cpp`, L1908)으로 **FMeshDescription으로 변환**한 뒤 `CreateMeshDescription` + `CommitMeshDescription`(L3264–3266). 호출부 주석: "Store the original fbx import data …"(`FbxSkeletalMeshImport.cpp`, L2036). 참고로 이 함수는 5.4부터 deprecated("Use CommitMeshDescription instead", `SkeletalMesh.h`, L666–667)라서 호출부가 경고를 끄고 쓴다(L2037–2039).

### 단계 8. 빌드: 원본 → FSkeletalMeshLODModel

- 8 — `IMeshBuilderModule::BuildSkeletalMesh` (호출 `FbxSkeletalMeshImport.cpp`, L2112) — `bUseLegacyMeshUtilities = false`(L2059)라서 새 빌드 경로를 탄다.
  - 8-1 — `FMeshBuilderModule::BuildSkeletalMesh` (`MeshBuilderModule.cpp`, L80–83) → `FSkeletalMeshBuilder::Build` (`SkeletalMeshBuilder.cpp`, L862)
  - 8-2 — `GetMeshDescription(LODIndex)`로 **저장된 MeshDescription을 읽음** (`SkeletalMeshBuilder.cpp`, L866) → `FSkeletalMeshImportData::CreateFromMeshDescription`으로 **중간 구조 복원** (L586) → `Options.FillOptions(LODInfo->BuildSettings)`, `Options.TargetPlatform = TargetPlatform` (L589–590)
  - 8-3 — `FContext::BuildLODModel` (`SkeletalMeshBuilder.cpp`, L280) → `CopyLODImportData` (L289; 본문 `SkeletalMeshLODImporterData.cpp`, L74) → `FMeshUtilities::BuildSkeletalMesh` (호출 L295; 본문 `MeshUtilities.cpp`, L4270)
    - 8-3-a — `PrepareSourceMesh` (`MeshUtilities.cpp`, L3964) — 겹치는 모서리 탐색 후 노멀/탄젠트 (재)계산 (`Skeletal_ComputeTangents`, L4002)
    - 8-3-b — `GenerateSkeletalRenderMesh` (`MeshUtilities.cpp`, L4014) — 면의 모서리마다 렌더 버텍스 후보(`FSoftSkinBuildVertex`) 생성, **인플루언스 제한**(L4122–4148), 동일 버텍스 병합 `SkeletalMeshTools::BuildSkeletalMeshChunks`(L4204), 뼈 개수 기준 청크 분할 `ChunkSkinnedVertices`(L4226)
    - 8-3-c — `FMeshUtilities::BuildSkeletalModelFromChunks` (`MeshUtilities.cpp`, L785; 호출 L4371) — 청크 → `FSkeletalMeshLODModel`의 섹션/인덱스 버퍼
  - 8-4 — `FinalizeContext` (`SkeletalMeshBuilder.cpp`, L808) — 모프 타깃, 대체 스킨 가중치, LOD 감소, 클로스 복원.

### 단계 9. 스켈레톤·역바인드 행렬

- 9-1 — `USkeletalMesh::CalculateInvRefMatrices` (호출 `FbxSkeletalMeshImport.cpp`, L2181) — 레퍼런스 포즈의 역행렬 캐시 (함수 본문은 읽지 않음; 이름과 용도로 볼 때 스키닝 식의 $B^{-1}$에 해당 — **추론**).
- 9-2 — `USkeleton` 에셋 생성 또는 기존 것에 `MergeAllBonesToBoneTree` (`FbxSkeletalMeshImport.cpp`, L2193–2222; `CreateAsset<USkeleton>` L2201).

### 단계 10. 렌더 데이터

- 10 — `FScopedSkeletalMeshPostEditChange`가 스코프를 벗어날 때 PostEditChange를 호출하도록 쌓아둔다 (`FbxSkeletalMeshImport.cpp`, L1889–1890; `FbxFactory.cpp`, L756). 물리 에셋 생성이 필요하면 즉시 `SkeletalMesh->Build()`(L2188) → `USkeletalMesh::Build` (`SkeletalMesh.cpp`, L2348) → `CacheDerivedData` (L5756) → `GetSkeletalMeshRenderData()->Cache(RunningPlatform, …)` (L5776). `FSkeletalMeshRenderData::Cache` 내부(LODModel → GPU 버퍼 변환)는 이번에 읽지 않아 **미확인**.

---

## 3. 질문 2 — 왜 FbxMesh를 바로 안 쓰고 중간 구조에 담나 (Raw → Cooked)

### 3-1. 큰 그림: 데이터가 거치는 4개의 층

| 층 | 형태 | 누가 만든다 | 어디에 산다 |
|---|---|---|---|
| ① DCC 원본 | `FbxScene`, `FbxMesh`, `FbxCluster` | FBX SDK (`Importer->Import`, `FbxMainImport.cpp` L1458) | 임포트 중 메모리에만 |
| ② Raw (임포트 데이터) | `FSkeletalMeshImportData` | `FillSkeletalMeshImportData` | 임포트 중 메모리 → **FMeshDescription으로 에셋에 저장** |
| ③ Built (에디터용 완성품) | `FSkeletalMeshLODModel` | `FMeshUtilities::BuildSkeletalMesh` | 에셋의 ImportedModel |
| ④ Cooked/Render | `FSkeletalMeshRenderData` | `FSkeletalMeshRenderData::Cache` | 플랫폼별 파생 데이터 |

헤더도 ②를 "Raw"로 부른다: `// Raw data bone.`, `// Raw data material.`, `FRawBoneInfluence // just weight, vertex, and Bone, sorted later....` (`SkeletalMeshLODImporterData.h`, L170, L186, L208–209). 클래스 설명은 "Container and importer for skeletal mesh (FBX file) data"(L325).

### 3-2. 코드가 직접 뒷받침하는 이유

**이유 A — FBX 파일 없이 다시 빌드할 수 있게 "원본"을 에셋에 보관한다.**
- 임포트 시 중간 데이터를 MeshDescription으로 저장한다 (`SkeletalMesh.cpp`, `USkeletalMesh::SaveLODImportedData`, L3254–3268). 호출부 주석: "Store the original fbx import data …" (`FbxSkeletalMeshImport.cpp`, L2036).
- 빌더는 FBX를 다시 열지 않고 **저장된 MeshDescription만** 읽는다 (`SkeletalMeshBuilder.cpp`, `FSkeletalMeshBuilder::Build`, L866), 그리고 그걸 다시 `FSkeletalMeshImportData`로 되돌려 쓴다 (`CreateFromMeshDescription`, L586).
- → 빌드 설정(예: 노멀 재계산, 탄젠트 방식)을 바꾸거나 다른 플랫폼용으로 다시 만들 때 원본 FBX가 필요 없다. (마지막 문장은 위 두 사실에서 바로 나오는 결론)

**이유 B — "지오메트리만" / "스키닝만" 재임포트.**
- 재임포트 옵션에 따라 `ReplaceSkeletalMeshGeometryImportData` / `ReplaceSkeletalMeshSkinningImportData`를 호출한다 (`FbxSkeletalMeshImport.cpp`, L1893–1905; 호출 L1898, L1903).
- 이 함수는 에셋에 저장돼 있던 **이전 임포트 데이터**를 `LoadLODImportedData`로 꺼내서 새 데이터와 합친다 (`SkeletalMeshLODImporterData.cpp`, `FSkeletalMeshImportData::ReplaceSkeletalMeshGeometryImportData`, L143–160; 리그 적용 `ApplyRigToGeo`, L230).
- 에셋은 소스 파일 슬롯을 "Geo and Skinning / Geometry / Skinning" 3개까지 기록한다 (`FbxSkeletalMeshImport.cpp`, L2155–2172; 기본 슬롯 L2165).
- → 중간 구조가 "위치·면" 부분과 "뼈·가중치" 부분을 따로 갈아끼울 수 있는 형태여야 이게 가능하다.

**이유 C — 빌드 결과가 플랫폼·빌드 설정에 따라 달라진다 (그래서 임포트 시점에 한 번 굽고 끝낼 수 없다).**
- 빌드 옵션에 **TargetPlatform**이 들어간다 (`SkeletalMeshBuilder.cpp`, L589–590).
- 인플루언스 상한은 `GetBoneInfluenceLimitForAsset(BoneInfluenceLimit, TargetPlatform)`으로 플랫폼별로 정해지고, `UseUnlimitedBoneInfluences(…, TargetPlatform)` 여부에 따라 8개로 잘린다 (`MeshUtilities.cpp`, `GenerateSkeletalRenderMesh`, L4063–4064, L4127–4146).
- 청크당 최대 뼈 수는 `FGPUBaseSkinVertexFactory::GHardwareMaxGPUSkinBones` (L4226).
- 노멀·탄젠트 재계산 여부, MikkTSpace 사용 여부, 겹침 판정 임계값(Threshold*)이 빌드 설정에서 온다 (`FbxSkeletalMeshImport.cpp`, L1925–1939; `MeshUtilities.cpp`, `PrepareSourceMesh`, L3980–4002).

**이유 D — 빌드 단계에서만 할 수 있는 "가공"이 많다.** 빌더가 실제로 하는 일:
- 노멀/탄젠트 계산 (`MeshUtilities.cpp`, L4002)
- **버텍스 분할/병합**: 면 모서리마다 렌더 버텍스 후보를 만든 뒤, 위치·UV·탄젠트·인플루언스가 모두 같은 것만 하나로 합친다 (`SkeletalMeshTools.cpp`, `AreSkelMeshVerticesEqual`, L20–60; `BuildSkeletalMeshChunks`, L74, 병합 판단 L163). 즉 위치가 같아도 UV 이음새나 하드 엣지에서는 버텍스가 갈라진다.
- 인플루언스 제한·양자화 (`MeshUtilities.cpp`, L4122–4180)
- 재질별 청크 → 섹션 (`SkeletalMeshTools.cpp`, L124–138), 뼈 수 기준 재분할 (`MeshUtilities.cpp`, L4226)

**이유 E — 빌드 코드는 FBX SDK를 모른다 (디커플링).**
- `FMeshUtilities::BuildSkeletalMesh`의 입력은 `FReferenceSkeleton`, `FVertInfluence`, `FMeshWedge`, `FMeshFace`, `FVector3f` 배열뿐이다 (`MeshUtilities.cpp`, L4270–4282). FBX 타입이 하나도 없다.
- FBX SDK 링크는 `UnrealEd` 모듈(`UnrealEd.Build.cs`, L353–356)과 `InterchangeFbxParser` 모듈(`InterchangeFbxParser.Build.cs`, L74)에만 있다. 빌더 쪽(`MeshBuilder`, `MeshUtilities`)의 Build.cs는 이번에 읽지 않았으므로 "FBX 의존이 없다"는 단정은 **미확인**이나, 소스에 FBX 타입이 등장하지 않는 것은 확인함.
- `FSkeletalMeshImportData` 자체가 `#if WITH_EDITOR` 블록 안에 선언돼 있다 (`SkeletalMeshLODImporterData.h`, L6) → 에디터 전용 데이터이며 게임 런타임에는 존재하지 않는다.

### 3-3. 추론 (코드에 명시는 없음)

- **추론**: FBX SDK 객체는 레이어·매핑 모드(`eByControlPoint`/`eByPolygonVertex`)·참조 모드(`eDirect`/`eIndexToDirect`)가 데이터마다 제각각이라 (`FbxSkeletalMeshImport.cpp`, L3699–3730, L3824–3828에서 매번 분기), 이를 한 번 "평평한 배열"로 정규화해 두면 이후 단계가 단순해진다. 이것이 중간 구조의 실무적 이점일 것이다.
- **추론**: 5.8에서 실제 저장 형식이 `FMeshDescription`인 점, Interchange FBX 파서도 `SkeletalMeshDescription` 모듈에 의존하는 점(`InterchangeFbxParser.Build.cs`, L49)으로 보아, UE의 장기 방향은 "임포터마다 다른 형식 → 공통 MeshDescription → 공통 빌더"로 보인다. 레거시 FBX 경로는 `FSkeletalMeshImportData`를 거쳐 MeshDescription으로 들어가는 "어댑터" 역할을 하는 셈이다.

---

## 4. 질문 3 — 중간 구조(FSkeletalMeshImportData)의 내용

### 4-1. 필드 표

선언: `SkeletalMeshLODImporterData.h`, `class FSkeletalMeshImportData`, L327–356.

| 필드 | 타입 | 쉬운 설명 | 어디서 채우나 |
|---|---|---|---|
| `Points` | `TArray<FVector3f>` | 컨트롤 포인트(위치만). 이미 UE 좌표계로 변환된 값 | `FillSkeletalMeshImportPoints` (`FbxSkeletalMeshImport.cpp`, L1600–1636) |
| `Wedges` | `TArray<FVertex>` | 삼각형 모서리 = `VertexIndex`(Points 번호) + `UVs[MAX_TEXCOORDS]` + `Color` + `MatIndex` (`.h` L225–231) | 삼각형 루프 (L3796–3931) |
| `Faces` | `TArray<FTriangle>` | 삼각형 = `WedgeIndex[3]` + `MatIndex` + `SmoothingGroups` + 모서리별 `TangentX/Y/Z[3]` (`.h` L106–119). **노멀·탄젠트는 웨지가 아니라 면의 모서리에 저장**된다 | 삼각형 루프 (L3663–3743) |
| `Influences` | `TArray<FRawBoneInfluence>` | (Weight, VertexIndex, BoneIndex) 3개짜리의 평평한 목록 (`.h` L209–213) | 클러스터 루프 (L3936–3969) |
| `RefBonesBinary` | `TArray<FBone>` | 뼈 = `Name`, `ParentIndex`, `NumChildren`, `BonePos`(바인드 포즈의 **부모 기준 로컬** 트랜스폼 + 길이/크기) (`.h` L187–194, `FJointPos` L88–96) | `ImportBones` (L1210–1392) |
| `Materials` | `TArray<FMaterial>` | 재질 이름(+ 찾은 UMaterial 약한 참조) | `FillSkeletalMeshImportData` (L1471–1508) |
| `MeshInfos` | `TArray<FMeshInfo>` | 원본에서 메시 덩어리별 이름·시작 점·점 개수. 주석: "렌더 버텍스 수가 아님"(`.h` L51) | `FillSkeletalMeshImportPoints` (L1594–1602) |
| `PointToRawMap` | `TArray<int32>` | 현재 점 번호 → 원래 임포트 점 번호 (`.h` L337 주석). 스무딩 그룹으로 점을 쪼개면 둘이 달라진다 | L1553–1566 |
| `NumTexCoords`, `MaxMaterialIndex`, `bHasVertexColors`, `bHasNormals`, `bHasTangents` | 스칼라 | UV 세트 수, 최대 재질 번호, 각 데이터 존재 여부. `bHasNormals/Tangents`가 거짓이면 빌드가 재계산 (`FbxSkeletalMeshImport.cpp`, L1930–1931) | L3577, L3590, L3612–3613 |
| `MorphTargets`, `MorphTargetModifiedPoints`, `MorphTargetNames` | | 모프 타깃(블렌드셰이프)별 별도 ImportData | 모프 임포트 경로 |
| `AlternateInfluences`, `AlternateInfluenceProfileNames` | | 대체 스킨 가중치 프로필 | |
| `VertexAttributes`, `VertexAttributeNames` | | 추가 버텍스 속성(현재 알파 전용 1채널) | L4008–4183 |

### 4-2. 가중치는 어떻게 저장되나 — 뼈 중심 → 점 중심

1) **FBX 원본은 뼈 중심(bone-centric)**. 클러스터 하나가 뼈 하나이고, "이 뼈가 움직이는 컨트롤 포인트 번호들 + 가중치들"을 가진다. UE는 클러스터를 돌면서 그대로 펼친다:

```cpp
// FbxSkeletalMeshImport.cpp, FillSkelMeshImporterFromFbx, L3956-3966
int32*  ControlPointIndices = Cluster->GetControlPointIndices();
double* Weights             = Cluster->GetControlPointWeights();
... Influences.Last().BoneIndex = BoneIndex; .Weight = Weights[i]; .VertexIndex = ExistPointNum + ControlPointIndices[i];
```

   - 뼈 번호는 정렬된 뼈 목록에서 찾는다 (`SortedLinks.Find(Link, BoneIndex)`, L3953).
   - Maya가 강체 바인딩 때 만드는 "CompensationCluster"는 건너뛴다 (L3942–3948).
   - 스킨이 없는 강체 메시는 모든 점에 (자기 노드 뼈, 가중치 1)을 넣는다 (L3970–3990).
   - 이 시점의 `Influences`는 **클러스터(뼈) 순서**로 쌓인 평평한 목록이다.

2) **정리 단계에서 점 중심(vertex-centric)으로 바뀐다** — `FLODUtilities::ProcessImportMeshInfluences` (`LODUtilities.cpp`, L62–217):
   - 정렬: 점 번호 오름차순, 같은 점 안에서는 **가중치 내림차순** (L66–79).
   - 1차 정규화: 점마다 가중치 합이 1이 되도록 나눈다. 주석: "Daz Studio 같은 DCC는 정규화하지 않는다" (L91–117).
   - 필터: `Weight >= BoneWeightThreshold` 이고 점당 개수 `< MAX_TOTAL_INFLUENCES`인 것만 남긴다 (L169–176). 내림차순 정렬 덕분에 **큰 가중치부터 최대 12개**가 살아남는다.
   - 남긴 것끼리 재정규화 (L143–153).
   - 가중치가 하나도 없는 점은 (뼈 0, 가중치 1)로 채운다 (L156–167, L186–214).
   - 초과 시 로그: "influence count of %d exceeds max count of %d. Influence truncation will occur." (L126–129)

3) **빌드 단계에서 버텍스마다 고정 길이 배열**이 된다 — `FSoftSkinBuildVertex::InfluenceBones[] / InfluenceWeights[]` (`MeshUtilities.cpp`, L4150–4180). 가중치는 `FBoneWeights::Create`를 거쳐 `GetRawWeight()`(정수 값)로 들어간다(L4160–4178). 정수 비트 폭은 이번에 확인하지 않음(**미확인**; 같은 파일 다른 곳 `uint16 InfluenceWeights[]` 사용, `SkeletalMeshBuilder.cpp` L126로 보아 16비트일 가능성 — **추론**).

### 4-3. "점당 최대 4/8개" 제한은 어디서?

상수 정의 (`GPUSkinPublicDefs.h`, L12–18):

| 상수 | 값 | 의미(주석 그대로) |
|---|---|---|
| `MAX_TOTAL_INFLUENCES` | 12 | 버텍스 하나가 가질 수 있는 최대 뼈 영향 수 |
| `EXTRA_BONE_INFLUENCES` | 8 | "extra bone influences" 사용 시 영향 수 |
| `MAX_INFLUENCES_PER_STREAM` | 4 | 버텍스 스트림 하나당 최대 영향 수 |

제한이 걸리는 곳은 **두 군데**다.

| 시점 | 위치 | 규칙 |
|---|---|---|
| 임포트 직후 (데이터 정리) | `LODUtilities.cpp`, `FLODUtilities::ProcessImportMeshInfluences`, L169–176 | 임계값 미만 제거, **최대 12개**, 재정규화 |
| 빌드 (렌더 버텍스 생성) | `MeshUtilities.cpp`, `GenerateSkeletalRenderMesh`, L4122–4148 | ① `min(개수, 12)` ② `min(개수, InfluenceLimit)` — `InfluenceLimit = GetBoneInfluenceLimitForAsset(BuildOptions.BoneInfluenceLimit, TargetPlatform)` (L4063–4064) ③ 개수 > 8 이고 플랫폼이 "unlimited bone influences"를 쓰지 않으면 **8개로 자름** + 경고 "requires unlimited bone influence mode on. Influence truncated to 8" (L4130–4148) |

"4개"는 `MAX_INFLUENCES_PER_STREAM`으로 정의돼 있으나, 이번에 읽은 빌드 코드 안에서 4로 자르는 지점은 찾지 못했다 → GPU 버퍼에 4개씩 나눠 담는 용도로 보이며(**추론**), 실제 사용처는 **미확인**. `GetBoneInfluenceLimitForAsset`의 기본값도 **미확인**.

### 4-4. 바인드 포즈는 FBX에서 어떻게 얻나

`FFbxImporter::ImportBones` (`FbxSkeletalMeshImport.cpp`, L1044–1423). 뼈마다 **전역 행렬** $G_i$를 아래 우선순위로 찾는다.

| 순위 | 출처 | 코드 |
|---|---|---|
| 1 | FBX의 **Bind Pose (FbxPose)** 에서 그 뼈의 행렬 | `PoseArray[PoseIndex]->GetMatrix(PoseLinkIndex)` (L1246–1259) |
| 2 | 그 뼈를 가리키는 **클러스터의 TransformLinkMatrix** | `Cluster->GetTransformLinkMatrix(GlobalsPerLink[LinkIndex])` (L1271–1281) |
| 3 | 노드의 현재 전역 트랜스폼 | `Link->EvaluateGlobalTransform()` (L1286–1289) |
| 덮어쓰기 | "Use T0 as ref pose" 옵션이면 **애니메이션 0프레임** 전역 행렬로 교체 | `GetNodeGlobalTransform(Link, 0)` (L1291–1300) |

바인드 포즈 확보 세부:
- `RetrievePoseFromBindPose` (L952–1042): 장면의 포즈 중 `IsBindPose()`인 것을 `IsValidBindPoseVerbose`로 검사. 실패하면 빠진 조상 노드를 0초 전역 행렬로 추가해 재검사, 그래도 안 되면 위쪽 Null 부모 기준으로 재검사. 주석: "IsValidBindPose doesn't work reliably"(L962).
- 그래도 없으면 기존 바인드 포즈를 지우고 `SdkManager->CreateMissingBindPoses(Scene)`로 다시 만들어 재시도 (L1105–1132).
- 끝내 없고 T0 옵션도 꺼져 있으면 경고 후 **자동으로 T0를 바인드 포즈로 사용** (L1149–1166).

전역 → 로컬:

$$
G_i \leftarrow G_i \cdot J \quad(\text{L1303},\ J = \text{JointPostConversionMatrix}),\qquad
L_i = G_{p(i)}^{-1}\,G_i \quad(\text{L1305–1310})
$$

- $p(i)$는 부모 뼈. 루트는 $L_{root} = G_{root}$ (L1312–1318).
- $L_i$에서 이동 T, 회전 Q, 스케일 S를 뽑아 `FFbxDataConverter`로 UE 좌표계로 바꿔 `Bone.BonePos.Transform`에 저장 (L1389–1391).
- NaN·0 스케일 검사 (L1325–1357), 다중 루트 금지 (L1232–1240), 중복 이름 금지 (L1173–1191).

즉 `RefBonesBinary`에는 **바인드 포즈의 부모 기준 로컬 트랜스폼**이 들어간다. 메시 정점(`Points`)은 이 바인드 포즈 상태 그대로의 위치다.

---

## 5. 질문 4 — 좌표계·단위 변환은 언제, 어디서?

### 5-1. 배경: 왜 변환이 필요한가

- FBX 파일은 저작 툴마다 "위쪽 축(Y-up/Z-up)", "앞쪽 축", "오른손/왼손 좌표계", "단위(cm/m/inch)"가 다르고, 이 정보가 `GlobalSettings`에 기록된다 (`ConvertScene`이 `GetAxisSystem()`/`GetSystemUnit()`로 읽음, `FbxMainImport.cpp` L1516–1517).
- UE는 **Z-up, 왼손 좌표계, 1 unit = 1 cm**.
- UE는 이 차이를 **두 단계**로 메운다.

### 5-2. 1단계: FBX SDK 안에서 장면 전체 변환 (흐름 2-3단계)

`FFbxImporter::ConvertScene` (`FbxMainImport.cpp`, L1499–1580), `ImportFromFile`에서 SDK 로드 직후 호출 (L1641).

- **축**: `bConvertScene` 옵션이 켜져 있으면 목표 축계를 **Up = Z, Front = −Y(−ParityOdd), 오른손**으로 잡는다 (L1533–1535). `bForceFrontXAxis`면 Front = +X(ParityEven) (L1536–1539).
  - 원본 축계와 다르면 `FbxRootNodeUtility::RemoveAllFbxRoots` 후 **`UnrealImportAxis.ConvertScene(Scene)`** (L1547–1550). 이건 FBX SDK의 `FbxAxisSystem::ConvertScene`이다.
  - 원본→목표 변환 행렬을 `AxisConversionMatrix`로 보관 (L1552–1556, L1568).
  - `bForceFrontXAxis`면 뼈용 보정 회전 `JointOrientationMatrix.SetR(-90, -90, 0)` (L1559–1562) → `SetJointPostConversionMatrix` (L1566). 이것이 질문 3의 $J$.
  - 주석(L1528–1532) 요지: "UE의 앞쪽은 +X인데 −Y를 앞쪽으로 쓰는 건 Maya/Max에서 +X를 보던 모델이 엔진에서도 +X를 보게 하려는 것. 다만 Max/Maya는 오른손(RHS)이고 UE는 왼손(LHS)이라 손 방향 뒤집기는 여기서 안 된다."
  - 즉 1단계가 끝나도 장면은 **아직 오른손 좌표계**다.
- **단위**: `bConvertSceneUnit`이 켜져 있고 파일 단위가 cm가 아니면 **`FbxSystemUnit::cm.ConvertScene(Scene)`** (L1573–1576). 주석: "FBX와 Unreal의 기본 단위는 cm" (L1570–1572).
  - 이 옵션이 꺼져 있으면 단위 변환은 일어나지 않는다. 뼈 길이에 쓰는 `ConvertDist`는 그냥 float 캐스팅뿐이다 (`FbxUtilsImport.cpp`, `ConvertDist`, L156–162).
- 마지막에 `Scene->GetAnimationEvaluator()->Reset()` (L1579) — 노드 트랜스폼이 바뀌었으니 평가 캐시 초기화.

### 5-3. 2단계: 데이터를 꺼낼 때마다 Y 뒤집기로 오른손 → 왼손 (흐름 5단계)

`FFbxDataConverter` (`FbxUtilsImport.cpp`), 선언 `FbxImporter.h` L474–520.

| 함수 | 변환 | 줄 |
|---|---|---|
| `ConvertPos` | $(x, y, z) \to (x, -y, z)$. 주석 "flip Y, then the right-handed axis system is converted to LHS" | L63–72 |
| `ConvertDir` | $(x, y, z) \to (x, -y, z)$ (노멀·탄젠트용) | L76–84 |
| `ConvertRotToQuat` | $(q_x, q_y, q_z, q_w) \to (q_x, -q_y, q_z, -q_w)$ | L141–151 |
| `ConvertScale` | 그대로 | L94–113 |
| `ConvertMatrix` | 행렬을 같은 Y 반전 규칙으로 부호 변경 (1행은 x·z·w 반전, 나머지 행은 y 반전) | L179–203 |

- 쿼터니언 규칙에 대한 수학적 해설(코드 해석): XZ 평면 거울 반사는 회전축 $(a_x,a_y,a_z)$를 $(a_x,-a_y,a_z)$로 바꾸고 회전 방향을 뒤집는다($\theta\to-\theta$). 그러면 $q=(\sin\tfrac{-\theta}{2}(a_x,-a_y,a_z),\ \cos\tfrac{\theta}{2}) = (-q_x, q_y, -q_z, q_w)$이고, 쿼터니언은 $q$와 $-q$가 같은 회전이므로 $(q_x,-q_y,q_z,-q_w)$와 같다.

이 변환이 실제로 적용되는 지점(모두 흐름 5단계):
- **정점 위치**: `TotalMatrix.MultT(Position)` 후 `Converter.ConvertPos` (`FbxSkeletalMeshImport.cpp`, L1623–1624). `TotalMatrix`는 `ComputeSkeletalMeshTotalMatrix`(`FbxMainImport.cpp`, L2048) → `ComputeTotalMatrix`(L2060–2098): 기본은 노드의 지오메트릭 트랜스폼(Max의 피벗 오프셋)만, `bTransformVertexToAbsolute`면 노드 전역 트랜스폼까지 곱한다 (L2095).
- **노멀/탄젠트**: 위치 행렬의 역전치 $(M^{-1})^{T}$로 변환 후 `ConvertDir` (L3650–3651, L3716–3730). **바이노멀은 부호를 한 번 더 뒤집어** `TangentY = -ConvertDir(...)` (L3724).
- **UV**: $v' = 1 - v$ (L3831). FBX는 V가 위로 증가, UE 텍스처 좌표는 아래로 증가하기 때문(이 이유 설명은 **추론**; 코드는 식만 있음).
- **음수 스케일**: 축 중 홀수 개가 음수면(`IsOddNegativeScale`, `FbxMainImport.cpp` L2100–2110) 삼각형 정점 순서를 뒤집는다 `UnrealVertexIndex = 2 - VertexIndex` (`FbxSkeletalMeshImport.cpp`, L3653, L3693).
  - 관찰: Y 반전(거울 변환) 자체에 대해서는 정점 순서를 뒤집는 코드가 이 함수에 없다. 그 보정이 다른 곳(예: UE의 앞면 판정 규약)에서 처리되는지는 **미확인**.
- **뼈**: 로컬 T/Q/S에 `ConvertPos/ConvertRotToQuat/ConvertScale` (L1389–1391), 전역 행렬에 $J$ 곱하기 (L1303).

### 5-4. 흐름 위의 위치 정리

| 흐름 단계 | 하는 변환 |
|---|---|
| 2-3 `ConvertScene` | 축(Z-up, −Y 또는 +X 앞, 아직 오른손) + 단위(cm) — **FBX 장면 전체에 한 번** |
| 5-1 `ImportBones` | 뼈 로컬 트랜스폼에 Y 반전, 쿼터니언 변환, $J$ 적용 |
| 5-3 `FillSkeletalMeshImportPoints` / 삼각형 루프 | 위치·노멀·탄젠트 Y 반전, 바이노멀 부호, UV $1-v$, 음수 스케일 시 감김 순서 반전 |
| 6단계 이후 | 이미 UE 좌표계. 추가 축 변환 없음 (이번에 읽은 범위 내) |

---

## 6. 질문 5 — Legacy FBX 임포터 vs Interchange

### 6-1. 버전

- **UE 5.8.2** (`Engine/Build/Build.version`).

### 6-2. 누가 Autodesk FBX SDK를 직접 부르나

| 경로 | FBX 읽는 라이브러리 | 근거 |
|---|---|---|
| **레거시** `UnFbx::FFbxImporter` (UnrealEd 모듈) | **Autodesk FBX SDK** 직접 호출 (`FbxManager::Create`, `FbxImporter::Create`, `Importer->Import`, `FbxAxisSystem::ConvertScene` …) | `UnrealEd.Build.cs` L353–356 `AddEngineThirdPartyPrivateStaticDependencies(Target, "FBX", …)`; 호출부 `FbxMainImport.cpp` L585, L1101, L1458, L1550 |
| **Interchange** `InterchangeFbxParser` 모듈 | **FBX SDK와 ufbx 둘 다 링크** | `InterchangeFbxParser.Build.cs` L74 `AddEngineThirdPartyPrivateStaticDependencies(Target, "FBX")`, L76–79 ufbx `0.21.2` include 경로 추가 |

- 들어 있는 SDK 버전: Autodesk FBX SDK **2020.2** (`Engine/Source/ThirdParty/FBX/2020.2`, `Autodesk-FBX-SDK-2020.2.tps`), ufbx **0.21.2**(빌드에 사용)와 0.18.2 (`Engine/Plugins/Interchange/Runtime/Source/Parsers/ThirdParty/uFBX/`).
- Interchange 파서 폴더에는 FBX SDK 래퍼로 보이는 `FbxAPI.cpp`, `FbxMesh.cpp`, `FbxScene.cpp`와 ufbx 쪽 `Ufbx/UfbxParser.cpp`, `Ufbx/UfbxMesh.cpp`, `Ufbx/UfbxScene.cpp`가 **나란히** 존재한다 (`Engine/Plugins/Interchange/Runtime/Source/Parsers/Fbx/Private/` 디렉터리 목록으로 확인).
- Interchange 파서는 `SkeletalMeshDescription`, `MeshDescription`에 의존한다 (`InterchangeFbxParser.Build.cs`, L46–50) → 레거시처럼 `FSkeletalMeshImportData`를 거치지 않고 MeshDescription으로 바로 가는 구조로 보인다(**추론**, 파서 소스 미열람).
- 에디터 설정상 Interchange FBX 번역기(`InterchangeFbxTranslator`)에 기본 파이프라인 `DefaultFBXOBJAssetsPipeline`이 연결돼 있다 (`BaseEngine.ini`, L3785).
- 레거시 팩토리는 Interchange FBX CVar 두 개가 모두 켜지면 `.fbx`를 등록하지 않는다 (`FbxFactory.cpp`, `UFbxFactory::GetFbxFormats`, L1204–1218).

### 6-3. 미확인 항목

- Interchange가 **실행 시 기본으로 FBX SDK와 ufbx 중 무엇을 쓰는지**(설정/CVar 이름과 기본값): `InterchangeFbxParser.cpp`, `InterchangeFbxSettings.h`, `InterchangeFbxTranslator.cpp`가 원격 파일 접근 도구의 폴더 깊이 제한(8단계 이상)으로 열리지 않아 **미확인**.
- `Interchange.FeatureFlags.Import.FBX` / `.ToLevel` CVar의 기본값: 정의 파일 미열람으로 **미확인**. 따라서 "5.8에서 .fbx를 드래그하면 기본적으로 어느 경로로 가는가"도 **미확인**.
- 레거시 코드 안 주석에 "the legacy FBX importer is deprecated" 예정을 시사하는 문장은 있다: "Hopefully, this solution will last until the legacy FBX importer is deprecated." (`FbxSkeletalMeshImport.cpp`, L2110).

---

## 7. 출처 목록

모두 UE 루트(`C:\Users\JUNGLE\Desktop\UnrealEngine-release`) 기준. 괄호 안은 본문에서 인용한 주요 심볼.

1. `Engine/Build/Build.version` — 버전 5.8.2
2. `Engine/Source/Editor/UnrealEd/Private/Fbx/FbxFactory.cpp` — `UFbxFactory::FactoryCreateFile`(L166), `GetFbxFormats`(L1201), `ImportFromFile` 호출(L414), `FillFbxSkelMeshArrayInScene` 호출(L446), `ImportSkeletalMesh` 호출(L750)
3. `Engine/Source/Editor/UnrealEd/Private/Fbx/FbxMainImport.cpp` — `FFbxImporter::FFbxImporter`(L574), `OpenFile`(L1081), `ImportFile`(L1424), `ConvertScene`(L1499), `ImportFromFile`(L1612), `ComputeSkeletalMeshTotalMatrix`(L2048), `ComputeTotalMatrix`(L2060), `IsOddNegativeScale`(L2100)
4. `Engine/Source/Editor/UnrealEd/Private/Fbx/FbxSkeletalMeshImport.cpp` — `DoUnSmoothVerts`(L643), `BuildSkeletonSystem`(L918), `RetrievePoseFromBindPose`(L952), `ImportBones`(L1044), `FillSkeletalMeshImportData`(L1425), `FillSkeletalMeshImportPoints`(L1582), `ImportSkeletalMesh`(L1756), `FillSkelMeshImporterFromFbx`(L3256)
5. `Engine/Source/Editor/UnrealEd/Private/Fbx/FbxUtilsImport.cpp` — `FFbxDataConverter::ConvertPos`(L63), `ConvertDir`(L76), `ConvertRotToQuat`(L141), `ConvertDist`(L156), `ConvertMatrix`(L179)
6. `Engine/Source/Editor/UnrealEd/Public/FbxImporter.h` — `FFbxDataConverter` 선언(L474–520), `bConvertScene/bForceFrontXAxis/bConvertSceneUnit`(L129–131)
7. `Engine/Source/Editor/UnrealEd/Private/ImportUtils/SkeletalMeshImportUtils.cpp` — `ProcessImportMeshSkeleton`(L165), `ProcessImportMeshInfluences`(L223)
8. `Engine/Source/Editor/UnrealEd/UnrealEd.Build.cs` — FBX 서드파티 의존(L353–356)
9. `Engine/Source/Runtime/Engine/Public/Rendering/SkeletalMeshLODImporterData.h` — `SkeletalMeshImportData::*` 구조체(L42–316), `FSkeletalMeshImportData`(L327–459)
10. `Engine/Source/Runtime/Engine/Private/Rendering/SkeletalMeshLODImporterData.cpp` — `CopyLODImportData`(L74), `ReplaceSkeletalMeshGeometryImportData`(L143), `ApplyRigToGeo`(L230), `GetMeshDescription`(L1908), `CreateFromMeshDescription`(L2678)
11. `Engine/Source/Runtime/Engine/Private/SkeletalMesh.cpp` — `USkeletalMesh::Build`(L2348), `SaveLODImportedData`(L3254), `CacheDerivedData`(L5756)
12. `Engine/Source/Runtime/Engine/Classes/Engine/SkeletalMesh.h` — `SaveLODImportedData`/`LoadLODImportedData` deprecated 5.4 (L663–667)
13. `Engine/Source/Runtime/Engine/Public/GPUSkinPublicDefs.h` — `MAX_TOTAL_INFLUENCES`, `EXTRA_BONE_INFLUENCES`, `MAX_INFLUENCES_PER_STREAM`(L12–18)
14. `Engine/Source/Developer/SkeletalMeshUtilitiesCommon/Private/LODUtilities.cpp` — `FLODUtilities::ProcessImportMeshInfluences`(L62)
15. `Engine/Source/Developer/MeshBuilder/Private/MeshBuilderModule.cpp` — `FMeshBuilderModule::BuildSkeletalMesh`(L80)
16. `Engine/Source/Developer/MeshBuilder/Private/SkeletalMeshBuilder.cpp` — `FContext::BuildLODModel`(L280), 컨텍스트 준비(L570–600), `FinalizeContext`(L808), `FSkeletalMeshBuilder::Build`(L862)
17. `Engine/Source/Developer/MeshUtilities/Private/MeshUtilities.cpp` — `BuildSkeletalModelFromChunks`(L785), `PrepareSourceMesh`(L3964), `GenerateSkeletalRenderMesh`(L4014), `FMeshUtilities::BuildSkeletalMesh`(L4270)
18. `Engine/Source/Developer/MeshUtilities/Private/SkeletalMeshTools.cpp` — `AreSkelMeshVerticesEqual`(L20), `BuildSkeletalMeshChunks`(L74), `ChunkSkinnedVertices`(L425)
19. `Engine/Plugins/Interchange/Runtime/Source/Parsers/Fbx/InterchangeFbxParser.Build.cs` — FBX SDK(L74) + ufbx 0.21.2(L76–79), MeshDescription 의존(L46–50)
20. `Engine/Plugins/Interchange/Runtime/Source/Parsers/Fbx/Private/` — 파일 목록만 확인 (FbxAPI.cpp, Ufbx/UfbxParser.cpp 등; 내용 미열람)
21. `Engine/Plugins/Interchange/Runtime/Source/Parsers/ThirdParty/uFBX/` — ufbx 0.18.2, 0.21.2
22. `Engine/Source/ThirdParty/FBX/` — Autodesk FBX SDK 2020.2
23. `Engine/Config/BaseEngine.ini` — Interchange FBX 번역기 파이프라인 설정(L3785)

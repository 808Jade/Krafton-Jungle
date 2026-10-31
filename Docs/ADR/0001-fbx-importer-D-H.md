# FBX Importer D~H단계 — 메시·뼈·가중치·바인드 포즈 채우기 (따라 하기)

상위 문서: [ADR-0001](0001-fbx-importer.md) · 이전: [B단계](0001-fbx-importer-B.md), [C단계](0001-fbx-importer-C.md)

## 이 문서 읽는 법

- **1장**: `FFbxImporter::Import`는 누가 부르나.
- **2장**: 완성 전/후 전체 흐름도.
- **3장**: D~H 한눈에.
- **4~8장**: 단계별 따라 하기. **파일 → 위치 → 코드** 순서. `// ▼ 추가` ~ `// ▲ 추가` 사이만 새로 쓴다.
- **9장**: 막히면. **부록**: `FbxImporter.cpp` 완성본(H까지 끝난 모습).
- 각 단계가 끝날 때마다 빌드하고 그 단계의 확인을 통과한 뒤 다음으로 간다.

---

## 1. `FFbxImporter::Import`는 누가 부르나

**에디터가 켜질 때, 에셋 스캔 중에, `.fbx.bin` 캐시가 없거나 오래됐을 때만** 불린다. 직접 부르는 코드는 없고, B단계에서 만든 "굽는 사람 자리"(`std::function`)를 통해 불린다.

```
FEngineLoop::PreInit                                        (Runtime/Launch/LaunchEngineLoop.cpp)
 ├ ① GEngine->RegisterAssetImporters()
 │     └ UEditorEngine: SetSkeletalMeshImporter(&FFbxImporter::Import)   ← 자리에 "등록"만 한다
 └ ② UAssetManager::Init → ScanAssets → LoadAsset(".fbx")
       └ UAssetManager::LoadFbxMesh                         (Runtime/Asset/AssetManager.cpp)
          └ LoadSkeletalMeshData(Path, SkeletalMeshImporter)
             └ Importer(Path, *Data)                         ← 여기서 "호출"된다 = FFbxImporter::Import
```

- ①에서 함수 주소를 자리에 넣고, ②에서 그 자리를 호출한다. Runtime 코드는 `FFbxImporter`라는 이름을 모른다(의존 방향 Editor → Runtime).
- 게임에서는 ①이 아무것도 등록하지 않으므로 절대 불리지 않는다.
- 에디터에서도 `.fbx.bin`이 최신이면 불리지 않는다(캐시를 읽고 끝).

---

## 2. 전체 흐름도

### 2-1. 변경 전 (FBX 작업 시작 전 엔진)

```
FEngineLoop::PreInit                          [에디터·게임 공통]
 ├ GEngine 생성
 ├ 렌더 장치·창
 └ UAssetManager::Init → ScanAssets("Assets")
     └ LoadAsset (확장자로 분기)
         ├ .png/.dds → LoadTexture
         ├ .json     → LoadFontAtlas
         ├ .obj      → LoadObjStaticMesh
         │              └ FObjImporter::LoadStaticMeshData
         │                 ├ .obj.bin 있으면 읽기
         │                 └ 없으면 Parse → Cook → .obj.bin 저장
         │              → CreateStaticMesh → UStaticMesh 등록
         └ .fbx      → (무시)
```

### 2-2. 변경 후 (H까지 완성)

```
FEngineLoop::PreInit
 ├ GEngine 생성 ─ 에디터: UEditorEngine / 게임: UGameEngine
 ├ 렌더 장치·창
 ├ GEngine->RegisterAssetImporters()                                    [B]
 │    에디터: SetSkeletalMeshImporter(&FFbxImporter::Import)            [C]
 │    게임:   (아무것도 안 함 → 굽는 사람 없음)
 └ UAssetManager::Init → ScanAssets → LoadAsset
     ├ .obj → (변경 없음)
     └ .fbx → LoadFbxMesh ─────────────────────────────── 위층        [B]
              ├ 1. 키 확인 (이미 등록됐으면 반환)
              ├ 2. LoadSkeletalMeshData ──────────────── 아래층       [B]
              │     ├ 최신성: .fbx가 .bin보다 새것인가 (에디터만)
              │     ├ .fbx.bin 읽기 → 있으면 반환 ◄── 게임은 여기서만 데이터를 얻는다
              │     ├ 굽는 사람 없음(게임) → 경고, 끝
              │     ├ 굽기: Importer(Path, Data) ═══════╗
              │     ├ .fbx.bin 저장                      ║
              │     └ 다시 읽어 개수 비교 (self-check)   ║                [H]
              └ 3. 만들기·등록                           ║
                    ├ CreateStaticMesh(바인드 포즈 메시) ║  GPU 버퍼·머티리얼   [D]
                    ├ 뼈 없음 → UStaticMesh 등록         ║                [D·H]
                    └ 뼈 있음 → USkeletalMesh 등록       ║                [H]
                                {Data, RenderMesh}       ║
                                                         ║
FFbxImporter::Import  (에디터 전용, FBX SDK) ◄═══════════╝
 ├ 1~4. Manager → 파일 열기 → Scene 읽기 → 파일 닫기                     [C]
 ├ 5. 구조 출력 (축·단위·텍스처·노드 트리)                               [C]
 ├ 6. 삼각형화                                                           [D]
 ├ 7. 축 변환 행렬 C                                                     [D]
 ├ 8. 뼈 배열 (부모 먼저)                                                [E]
 ├ 9. 메시: 정점 펼치기 · 머티리얼 · 섹션 · AABB                         [D]
 ├ 10. 가중치: 뼈 중심 → 정점 중심 뒤집기                                [F]
 ├ 11. 바인드 포즈 → InverseBindPose                                     [G]
 └ 12. Validate → true
```

### 2-3. 누가 무엇을 아나 (완성 후)

| | FBX SDK | `FFbxImporter` | `FSkeletalMeshData` / `.fbx.bin` | `USkeletalMesh` |
|---|---|---|---|---|
| Editor (`HitoriEditor`) | ✅ | ✅ | ✅ | ✅ |
| Runtime (`HitoriEngine`) | ❌ | ❌ | ✅ | ✅ |
| Game (`HitoriGame`) | ❌ | ❌ | ✅ (`.bin`만 읽음) | ✅ |

---

## 3. D~H 한눈에

| 단계 | 채우는 것 | SDK가 주는 것 | 우리가 하는 일 | 눈으로 확인 | `BakeVersion` |
|---|---|---|---|---|---|
| **D** | `Mesh` | 삼각형화, 정점·노멀·UV 값 | 축 변환, 정점 펼치기, 감김 반전, 섹션, 텍스처 경로 | **화면에 T-Pose 메시** | 2 |
| **E** | `Bones` | 노드 트리 | 뼈만 골라 부모 먼저 배열 | 뼈 계층 로그 | 3 |
| **F** | `SkinWeights` | Cluster별 (정점, 비율) | 정점 중심으로 뒤집기, 정렬·12개·재정규화 | `Validate` 통과, 가중치 로그 | 4 |
| **G** | `InverseBindPose` | 뼈의 전역 행렬 | 축 변환, 역행렬, Cluster 없는 뼈 대체 | 뼈 위치가 메시 안에 있는가 | 5 |
| **H** | 등록 | — | 뼈 유무로 Static/Skeletal 분기, 저장 검사 | 로그 | — |

**`BakeVersion`을 단계마다 올리는 이유**: Importer의 출력이 바뀌어도 `.fbx` 파일은 그대로라 최신성 검사가 걸리지 않는다. 버전을 올려야 옛 `.bin`을 버리고 다시 굽는다.

### 시작 전에 바뀐 결정 하나 (A-6, D-1)

ADR에는 "D-1: SDK의 `ConvertScene`으로 축 변환"이라고 적었지만, **`ConvertScene`을 쓰지 않고 우리가 한 번에 변환한다.**

- `ConvertScene`은 노드 Transform만 고친다. 그런데 Cluster 안에 저장된 바인드 행렬(G에서 씀)까지 같은 규칙으로 바뀌는지는 SDK 문서만으로 확실하지 않다.
- 대신 **파일 공간 그대로 전부 읽고, 마지막에 같은 행렬 C 하나로** 정점·노멀·바인드 행렬을 모두 바꾼다. 그러면 정점과 뼈가 같은 변환을 받는 것이 코드로 보장된다.
- Y-Up 파일의 C는 OBJ의 `ToEngine`(`(x, y, z) → (-z, x, y)`)과 **같은 규칙**이다. Z-Up 파일은 UE와 같은 Y 반전이다.

---

## 4. D단계 — 보이는 것: 메시

### 생각의 흐름

```
D-1. 파일 축이 엔진 축과 다르다           → 축 변환 행렬 C
D-2. FBX 폴리곤은 사각형일 수 있다         → SDK로 삼각형화
D-3. 위치는 ControlPoint, UV·노멀은 꼭짓점 → 정점 펼치기 (OBJ의 v/vt/vn과 같은 문제)
D-4. 머티리얼이 여러 개다                  → 슬롯·섹션, 텍스처 경로
D-5. 구운 것을 저장해야 한다               → Bake 본문
D-6. 아직 뼈가 없다                        → UStaticMesh로 등록 → 화면 확인
```

### 작업 D-0. 구조체 마무리 + 검사 함수

**파일:** `Source/Runtime/Render/SkeletalMeshData.h` — 전체를 아래로 바꾼다

```cpp
#pragma once

#include <cmath>

#include "Render/StaticMeshData.h"
#include "Math/Matrix.h"

// FBX에서 구워 낸 SkeletalMesh 데이터. 에디터가 채우고 런타임이 읽는다.
// 규칙
// 1. SkinWeights[i]는 Mesh.Vertices[i]의 짝 (순서·개수 동일)
// 2. Bones는 부모가 자식보다 앞
// 3. 정점은 모델 공간 (피부를 붙인 순간)
// 4. 스키닝은 행벡터: v' = v * InverseBindPose * BoneTransform

// 정점 하나가 따라갈 수 있는 뼈 수 (UE 임포트 상한과 같음). 안 쓰는 칸은 Weight 0
constexpr int32 MAX_TOTAL_INFLUENCES = 12;

struct FSkinWeight
{
	int32 BoneIndex[MAX_TOTAL_INFLUENCES] = {};
	float Weight[MAX_TOTAL_INFLUENCES] = {};
};

struct FBoneInfo
{
	FString Name;
	int32 ParentIndex = -1;      // 루트는 -1
	FMatrix InverseBindPose;     // G단계에서 채움
};

struct FSkeletalMeshData
{
	FStaticMeshData Mesh;              // T-Pose 정점·인덱스·섹션·머티리얼·AABB
	TArray<FSkinWeight> SkinWeights;
	TArray<FBoneInfo> Bones;

	// 메시 검사 + 규칙 1·2 + 가중치 범위·합. 비어 있는 배열은 통과 (D·E 단계)
	bool Validate(FString& OutError) const
	{
		// 1. 메시
		if (!Mesh.Validate(OutError))
		{
			return false;
		}

		// 2. 규칙 1: 가중치와 정점 짝
		if (!SkinWeights.IsEmpty() && SkinWeights.Num() != Mesh.Vertices.Num())
		{
			OutError = "skin weight count != vertex count";
			return false;
		}

		// 3. 규칙 2: 부모가 먼저
		for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
		{
			if (Bones[BoneIndex].ParentIndex >= BoneIndex)
			{
				OutError = "bone parent must come before child";
				return false;
			}
		}

		// 4. 가중치: 뼈 번호 범위, 합 = 1
		for (const FSkinWeight& Skin : SkinWeights)
		{
			float Sum = 0.0f;
			for (int32 Slot = 0; Slot < MAX_TOTAL_INFLUENCES; ++Slot)
			{
				if (Skin.Weight[Slot] > 0.0f && (Skin.BoneIndex[Slot] < 0 || Skin.BoneIndex[Slot] >= Bones.Num()))
				{
					OutError = "bone index out of range";
					return false;
				}
				Sum += Skin.Weight[Slot];
			}
			if (std::abs(Sum - 1.0f) > 1.0e-3f)
			{
				OutError = "weights do not sum to 1";
				return false;
			}
		}
		return true;
	}
};
```

- `InverseBindPose`를 지금 넣는 이유: Bake 형식(D-5)을 한 번에 완성해 두면, E~G에서는 값만 채우고 `BakeVersion`만 올리면 된다.
- `Validate`는 비어 있는 배열을 통과시킨다. 그래서 D에서 지금 써도 되고, E·F에서 데이터가 들어오면 자동으로 검사가 걸린다.

### 작업 D-1~D-4. 메시 굽기 함수들

**파일:** `Source/Editor/Fbx/FbxImporter.cpp` — 수정

(1) **include**: 기존 include 묶음을 아래로 바꾼다.

```cpp
#include <fbxsdk.h>          // FBX SDK 전체 (FbxManager, FbxScene, FbxNode, FbxMesh, FbxSkin ...)
#include <algorithm>
#include <filesystem>
#include <format>
#include <map>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>
```

(2) **함수 위치**: 익명 네임스페이스 안, C단계의 `PrintSceneInfo` 함수 바로 아래(네임스페이스를 닫는 `}` 위)

```cpp
	// ▼ 추가 (D)
	// ===================== D. 메시 =====================

	// 파일 축 → 엔진 축 행렬 C (행벡터: p_engine = p_file · C)
	// - 엔진: X 앞, Y 오른쪽, Z 위, 왼손
	// - Y-Up 오른손 파일: (x, y, z) → (-z, x, y)   ObjImporter::ToEngine과 같은 규칙
	// - Z-Up 오른손 파일: (x, y, z) → (x, -y, z)   UE와 같은 규칙 (Y 반전)
	// - 두 경우 모두 거울 변환(손 방향이 바뀜) → 삼각형 감김도 뒤집어야 한다 (AppendMesh 7번)
	FMatrix MakeAxisConversion(FbxScene* Scene, const FString& FbxPath)
	{
		// 1. 파일의 위쪽 축과 손 방향
		const FbxAxisSystem& Axis = Scene->GetGlobalSettings().GetAxisSystem();
		int UpSign = 0;
		const FbxAxisSystem::EUpVector Up = Axis.GetUpVector(UpSign);

		// [if] 지원하지 않는 축
		// - 무엇: X-Up이거나 왼손 파일 (드묾)
		// - 처리: 경고만 남기고 Y-Up 규칙으로 진행 → 화면에서 방향 확인
		if (Axis.GetCoorSystem() != FbxAxisSystem::eRightHanded || (Up != FbxAxisSystem::eYAxis && Up != FbxAxisSystem::eZAxis))
		{
			HTR_LOG(Warning, "[FBX] {}: unsupported axis system, treated as Y-Up right-handed", FbxPath);
		}

		// [if] Z-Up 파일 (3ds Max, UE 내보내기 등)
		if (Up == FbxAxisSystem::eZAxis)
		{
			return FMatrix(
				1.0f,  0.0f, 0.0f, 0.0f,     // 파일 X → 엔진 X
				0.0f, -1.0f, 0.0f, 0.0f,     // 파일 Y → 엔진 -Y
				0.0f,  0.0f, 1.0f, 0.0f,     // 파일 Z → 엔진 Z
				0.0f,  0.0f, 0.0f, 1.0f);
		}

		// 2. Y-Up 파일 (Maya, Blender 기본, Mixamo 등)
		return FMatrix(
			 0.0f, 1.0f, 0.0f, 0.0f,         // 파일 X → 엔진 Y
			 0.0f, 0.0f, 1.0f, 0.0f,         // 파일 Y → 엔진 Z
			-1.0f, 0.0f, 0.0f, 0.0f,         // 파일 Z → 엔진 -X
			 0.0f, 0.0f, 0.0f, 1.0f);
	}

	// 점·방향 변환: p_engine = p_file · C (x·0행 + y·1행 + z·2행)
	FVector ConvertVector(const FbxVector4& V, const FMatrix& C)
	{
		const float X = static_cast<float>(V[0]);
		const float Y = static_cast<float>(V[1]);
		const float Z = static_cast<float>(V[2]);
		return FVector(
			X * C.M[0][0] + Y * C.M[1][0] + Z * C.M[2][0],
			X * C.M[0][1] + Y * C.M[1][1] + Z * C.M[2][1],
			X * C.M[0][2] + Y * C.M[1][2] + Z * C.M[2][2]);
	}

	// 행렬 변환: 엔진 공간의 같은 변환 = Cᵀ · F · C
	// - FbxAMatrix는 우리 FMatrix처럼 이동이 마지막 행(3행)에 있어 원소를 그대로 복사한다 (UE도 같음)
	// - 검산: p_engine · (Cᵀ·F·C) = (p_file·C)·Cᵀ·F·C = p_file · F · C
	FMatrix ConvertMatrix(const FbxAMatrix& In, const FMatrix& C)
	{
		FMatrix F;
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Col = 0; Col < 4; ++Col)
			{
				F.M[Row][Col] = static_cast<float>(In.Get(Row, Col));
			}
		}
		return C.GetTransposed() * F * C;
	}

	// 메시 노드 수집 (트리 순회)
	void CollectMeshNodes(FbxNode* Node, std::vector<FbxNode*>& OutNodes)
	{
		// [if] 메시 꼬리표가 있는 노드만
		if (Node->GetMesh())
		{
			OutNodes.push_back(Node);
		}

		// [for] 자식으로 재귀
		for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
		{
			CollectMeshNodes(Node->GetChild(ChildIndex), OutNodes);
		}
	}

	// 피부를 붙인 순간의 메시 전역 행렬 (A-5: 정점을 모델 공간으로 옮기는 행렬)
	FbxAMatrix GetMeshBindMatrix(FbxNode* Node, FbxMesh* Mesh)
	{
		// 1. Geometric 변환: 메시에만 붙는 오프셋 (자식 노드에는 안 물려줌)
		const FbxAMatrix Geometry(
			Node->GetGeometricTranslation(FbxNode::eSourcePivot),
			Node->GetGeometricRotation(FbxNode::eSourcePivot),
			Node->GetGeometricScaling(FbxNode::eSourcePivot));

		// 2. 기본값: 0프레임 전역 행렬
		FbxAMatrix Global = Node->EvaluateGlobalTransform();

		// [if] Skin이 있으면 Cluster에 기록된 "붙인 순간" 메시 행렬로 덮어쓰기
		// - 무엇: 모든 Cluster가 같은 메시 행렬을 가지므로 0번 하나만 본다
		// - 이유: 0프레임 자세가 바인드 자세와 다른 파일 대비
		if (Mesh->GetDeformerCount(FbxDeformer::eSkin) > 0)
		{
			FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(0, FbxDeformer::eSkin));
			if (Skin->GetClusterCount() > 0)
			{
				Skin->GetCluster(0)->GetTransformMatrix(Global);
			}
		}

		// 3. 전역 × Geometric (FBX 행렬 곱 순서)
		return Global * Geometry;
	}

	// 머티리얼 속성에 연결된 텍스처의 저장소 경로 (A-10)
	FString FindTexturePath(const FbxSurfaceMaterial* Material, const char* PropertyName, const FString& FbxPath)
	{
		// 1. 속성 → 연결된 이미지 파일 텍스처
		const FbxProperty Property = Material->FindProperty(PropertyName);
		if (!Property.IsValid())
		{
			return {};
		}
		const FbxFileTexture* Texture = Property.GetSrcObject<FbxFileTexture>(0);
		if (!Texture)
		{
			return {};
		}

		// 2. 후보 경로: .fbx 기준 상대 경로 → 같은 폴더의 파일 이름
		const std::filesystem::path BaseDir = std::filesystem::path(FbxPath).parent_path();
		const std::filesystem::path Candidates[] =
		{
			BaseDir / Texture->GetRelativeFileName(),
			BaseDir / std::filesystem::path(Texture->GetFileName()).filename(),
		};

		// [for] 실제로 있는 첫 후보를 "Assets/..." 형태로 (OBJ의 TransformPath와 같은 형태)
		for (const std::filesystem::path& Candidate : Candidates)
		{
			std::error_code Error;
			if (std::filesystem::exists(Candidate, Error))
			{
				return Candidate.lexically_normal().generic_string();
			}
		}

		HTR_LOG(Warning, "[FBX] {}: texture not found ({})", FbxPath, Texture->GetFileName());
		return {};
	}

	// FBX 머티리얼 → 엔진 머티리얼 슬롯
	FStaticMaterialSlot MakeMaterialSlot(const FbxSurfaceMaterial* Material, const FString& FbxPath)
	{
		FStaticMaterialSlot Slot;
		Slot.Name = Material->GetName();

		// 1. BaseColor 텍스처
		Slot.DiffuseTexturePath = FindTexturePath(Material, FbxSurfaceMaterial::sDiffuse, FbxPath);

		// 2. Normal 텍스처 (NormalMap 없으면 Bump)
		Slot.NormalTexturePath = FindTexturePath(Material, FbxSurfaceMaterial::sNormalMap, FbxPath);
		if (Slot.NormalTexturePath.empty())
		{
			Slot.NormalTexturePath = FindTexturePath(Material, FbxSurfaceMaterial::sBump, FbxPath);
		}

		// [if] 텍스처가 없으면 Diffuse 색을 BaseColor로
		if (Slot.DiffuseTexturePath.empty())
		{
			const FbxProperty Diffuse = Material->FindProperty(FbxSurfaceMaterial::sDiffuse);
			if (Diffuse.IsValid())
			{
				const FbxDouble3 Color = Diffuse.Get<FbxDouble3>();
				Slot.BaseColor = FVector4(static_cast<float>(Color[0]), static_cast<float>(Color[1]), static_cast<float>(Color[2]), 1.0f);
			}
		}
		return Slot;
	}

	// 펼친 정점 하나가 어느 메시의 어느 ControlPoint에서 왔나 (F에서 가중치를 찾을 때 사용)
	struct FVertexSource
	{
		FbxMesh* Mesh = nullptr;
		int32 ControlPoint = -1;
	};

	// 메시 노드 하나를 OutMesh 뒤에 이어 붙인다 (메시가 여러 개면 여러 번 호출)
	void AppendMesh(FbxNode* Node, const FString& FbxPath, const FMatrix& Axis,
		FStaticMeshData& OutMesh, std::vector<std::vector<uint32>>& OutSlotTriangles, std::vector<FVertexSource>& OutSources)
	{
		FbxMesh* Mesh = Node->GetMesh();

		// 1. 정점을 모델 공간으로 옮길 행렬 (A-5)
		const FbxAMatrix Bind = GetMeshBindMatrix(Node, Mesh);

		// 2. 머티리얼 슬롯: 이 메시의 0번 머티리얼 = 전체의 SlotBase번
		const int32 SlotBase = OutMesh.MaterialSlots.Num();
		const int32 MaterialCount = Node->GetMaterialCount();

		// [for] 이 노드의 머티리얼 → 슬롯
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			OutMesh.MaterialSlots.Add(MakeMaterialSlot(Node->GetMaterial(MaterialIndex), FbxPath));
		}

		// [if] 머티리얼 없는 메시 → 기본 슬롯 하나
		if (MaterialCount == 0)
		{
			FStaticMaterialSlot DefaultSlot;
			DefaultSlot.Name = "Default";
			OutMesh.MaterialSlots.Add(DefaultSlot);
		}
		OutSlotTriangles.resize(OutMesh.MaterialSlots.Num());

		// 3. 폴리곤별 머티리얼 번호 (eByPolygon이 아니면 전부 0번)
		const FbxGeometryElementMaterial* MaterialElement = Mesh->GetElementMaterial();
		const bool bMaterialPerPolygon = MaterialElement && MaterialElement->GetMappingMode() == FbxLayerElement::eByPolygon;

		// 4. UV 세트 (첫 번째만)
		FbxStringList UVSetNames;
		Mesh->GetUVSetNames(UVSetNames);
		const char* UVSet = UVSetNames.GetCount() > 0 ? UVSetNames.GetStringAt(0) : nullptr;

		// 5. 고유 정점 표 (A-9: ControlPoint, 머티리얼, 노멀, UV가 같으면 같은 정점)
		using FVertexKey = std::tuple<int32, int32, float, float, float, float, float>;
		std::map<FVertexKey, uint32> VertexIndexByKey;

		// [for] 폴리곤 순회 (삼각형화 후라 꼭짓점 3개)
		for (int32 Polygon = 0; Polygon < Mesh->GetPolygonCount(); ++Polygon)
		{
			// 6. 이 삼각형의 슬롯
			int32 LocalMaterial = bMaterialPerPolygon ? MaterialElement->GetIndexArray().GetAt(Polygon) : 0;
			if (LocalMaterial < 0 || LocalMaterial >= std::max(MaterialCount, 1))
			{
				LocalMaterial = 0;
			}
			const int32 Slot = SlotBase + LocalMaterial;

			// [for] 꼭짓점 3개
			// - 무엇: 위치는 ControlPoint에서, 노멀·UV는 "이 폴리곤의 이 꼭짓점"에서
			// - SDK 도우미(GetPolygonVertexNormal/UV)가 Layer Element의 mapping·reference 모드를 대신 풀어 준다
			uint32 Corners[3] = {};
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 ControlPoint = Mesh->GetPolygonVertex(Polygon, Corner);

				FbxVector4 Normal(0.0, 0.0, 1.0, 0.0);
				Mesh->GetPolygonVertexNormal(Polygon, Corner, Normal);

				FbxVector2 UV(0.0, 0.0);
				bool bUnmapped = false;
				if (UVSet)
				{
					Mesh->GetPolygonVertexUV(Polygon, Corner, UVSet, UV, bUnmapped);
				}

				// [if] 이미 만든 정점이면 재사용
				const FVertexKey Key{ ControlPoint, Slot,
					static_cast<float>(Normal[0]), static_cast<float>(Normal[1]), static_cast<float>(Normal[2]),
					static_cast<float>(UV[0]), static_cast<float>(UV[1]) };
				const auto Found = VertexIndexByKey.find(Key);
				if (Found != VertexIndexByKey.end())
				{
					Corners[Corner] = Found->second;
					continue;
				}

				// 새 정점: 모델 공간 → 엔진 축, UV v 뒤집기(OBJ와 동일)
				FVertex Vertex;
				Vertex.Position = ConvertVector(Bind.MultT(Mesh->GetControlPointAt(ControlPoint)), Axis);
				Vertex.Normal = ConvertVector(Bind.MultR(Normal), Axis).GetSafeNormal(1.0e-12f);
				Vertex.UV = FVector2(static_cast<float>(UV[0]), 1.0f - static_cast<float>(UV[1]));

				const uint32 NewIndex = static_cast<uint32>(OutMesh.Vertices.Num());
				OutMesh.Vertices.Add(Vertex);
				OutSources.push_back({ Mesh, ControlPoint });   // OutMesh.Vertices와 같은 순서·개수
				VertexIndexByKey.emplace(Key, NewIndex);
				Corners[Corner] = NewIndex;
			}

			// 7. 감김 반전 (0, 2, 1): 축 변환이 거울 변환이라서
			std::vector<uint32>& Triangles = OutSlotTriangles[Slot];
			Triangles.push_back(Corners[0]);
			Triangles.push_back(Corners[2]);
			Triangles.push_back(Corners[1]);
		}
	}

	// 슬롯별 삼각형 → Indices·Sections, AABB
	void FinishMesh(const std::vector<std::vector<uint32>>& SlotTriangles, FStaticMeshData& OutMesh)
	{
		// [for] 슬롯 순서대로 이어 붙이고 섹션 기록 (빈 슬롯은 섹션 없음)
		for (int32 Slot = 0; Slot < static_cast<int32>(SlotTriangles.size()); ++Slot)
		{
			if (SlotTriangles[Slot].empty())
			{
				continue;
			}
			FStaticMeshSection Section;
			Section.StartIndex = static_cast<uint32>(OutMesh.Indices.Num());
			Section.IndexCount = static_cast<uint32>(SlotTriangles[Slot].size());
			Section.MaterialSlotIndex = static_cast<uint32>(Slot);
			for (uint32 Index : SlotTriangles[Slot])
			{
				OutMesh.Indices.Add(Index);
			}
			OutMesh.Sections.Add(Section);
		}

		// [if] AABB (정점이 있을 때만)
		if (!OutMesh.Vertices.IsEmpty())
		{
			FVector Min = OutMesh.Vertices[0].Position;
			FVector Max = Min;
			for (const FVertex& Vertex : OutMesh.Vertices)
			{
				Min = FVector(std::min(Min.X, Vertex.Position.X), std::min(Min.Y, Vertex.Position.Y), std::min(Min.Z, Vertex.Position.Z));
				Max = FVector(std::max(Max.X, Vertex.Position.X), std::max(Max.Y, Vertex.Position.Y), std::max(Max.Z, Vertex.Position.Z));
			}
			OutMesh.AABB.Min = Min;
			OutMesh.AABB.Max = Max;
		}
	}
	// ▲ 추가 (D)
```

(3) **`FFbxImporter::Import`**: C단계 "6. Cluster 뼈 수집 → 노드 트리 출력" 아래부터 함수 끝까지를 아래로 바꾼다.

```cpp
	// 6. Cluster 뼈 수집 → 노드 트리 출력
	std::unordered_set<FString> ClusterBones;
	CollectClusterBones(Scene->GetRootNode(), ClusterBones);
	PrintNode(Scene->GetRootNode(), 0, ClusterBones);

	// ▼ 바꿈 (D)
	// 7. 삼각형화 (D-2): 메시를 삼각형 메시로 교체. Skin은 새 메시로 옮겨진다
	FbxGeometryConverter Converter(Manager.get());
	if (!Converter.Triangulate(Scene, true))
	{
		HTR_LOG(Warning, "[FBX] {}: triangulate failed on some meshes", FbxPath);
	}

	// 8. 축 변환 행렬 (D-1)
	const FMatrix Axis = MakeAxisConversion(Scene, FbxPath);

	// 9. 메시 (D-3, D-4)
	std::vector<FbxNode*> MeshNodes;
	CollectMeshNodes(Scene->GetRootNode(), MeshNodes);

	const bool bAnySkin = !ClusterBones.empty();
	std::vector<std::vector<uint32>> SlotTriangles;
	std::vector<FVertexSource> Sources;

	// [for] 메시 노드마다 이어 붙이기
	for (FbxNode* Node : MeshNodes)
	{
		// [if] Skin 있는 파일에서 Skin 없는 메시는 제외 (A-10)
		if (bAnySkin && Node->GetMesh()->GetDeformerCount(FbxDeformer::eSkin) == 0)
		{
			HTR_LOG(Warning, "[FBX] {}: mesh '{}' has no skin, skipped", FbxPath, Node->GetName());
			continue;
		}
		AppendMesh(Node, FbxPath, Axis, OutData.Mesh, SlotTriangles, Sources);
	}
	FinishMesh(SlotTriangles, OutData.Mesh);

	// 10. 검사
	FString Error;
	if (!OutData.Validate(Error))
	{
		HTR_LOG(Error, "[FBX] {}: {}", FbxPath, Error);
		return false;
	}

	HTR_LOG(Info, "[FBX] {}: vertices {}, indices {}, sections {}, materials {}", FbxPath,
		OutData.Mesh.Vertices.Num(), OutData.Mesh.Indices.Num(), OutData.Mesh.Sections.Num(), OutData.Mesh.MaterialSlots.Num());
	return true;
	// ▲ 바꿈 (D)
}
```

- C단계의 `(void)OutData;`와 `mesh data not implemented yet` 줄은 지운다.
- **C에서 만든 `ClusterBones`를 D가 그대로 쓴다**: Skin이 있는 파일인지(`bAnySkin`) 판단.

### 작업 D-5. Bake 본문

#### D-5-1. StaticMesh 본문을 함께 쓰게 열기

**파일:** `Source/Runtime/Asset/ObjImporter/StaticMeshBake.h` — 수정

```cpp
#pragma once

// ▼ 추가
#include <fstream>
// ▲ 추가

#include "Core/Types.h"
#include "Core/EngineString.h"

struct FStaticMeshData;

// Cook이 끝난 FStaticMeshData를 .bin으로 저장하고 읽는다.
class FStaticMeshBake
{
public:
	static TUniquePtr<FStaticMeshData> ReadBaked(const FString& BinPath);
	static void WriteBaked(const FString& BinPath, const FStaticMeshData& Data);

	// ▼ 추가
	// 헤더 없이 메시 본문만 읽고 쓴다 (SkeletalMeshBake가 같이 쓴다)
	static bool ReadMeshBody(std::ifstream& In, FStaticMeshData& Data);
	static void WriteMeshBody(std::ofstream& Out, const FStaticMeshData& Data);
	// ▲ 추가
};
```

**파일:** `Source/Runtime/Asset/ObjImporter/StaticMeshBake.cpp` — 수정

`ReadBaked`·`WriteBaked` 안에서 **헤더 다음부터 AABB까지**를 새 함수 두 개로 옮긴다. 바이트 형식은 그대로라 `BakeVersion{ 5 }`는 올리지 않는다.

```cpp
bool FStaticMeshBake::ReadMeshBody(std::ifstream& In, FStaticMeshData& Data)
{
	constexpr uint32 MaxCount = 1000000;

	// 1. 배열 4개
	if (!ReadArray<FVertex>(In, Data.Vertices, MaxCount) ||
		!ReadArray<FLODSourceVertex>(In, Data.LODSourceVertices, MaxCount) ||
		!ReadArray<uint32>(In, Data.Indices, MaxCount) ||
		!ReadArray<FStaticMeshSection>(In, Data.Sections, MaxCount))
	{
		return false;
	}

	// 2. 머티리얼 슬롯
	uint32 MaterialSlotsNum = 0;
	if (!ReadPod<uint32>(In, MaterialSlotsNum))
	{
		return false;
	}
	for (uint32 i = 0; i < MaterialSlotsNum; ++i)
	{
		FStaticMaterialSlot Slot{};
		if (!ReadString(In, Slot.Name) || !ReadPod<FVector4>(In, Slot.BaseColor) || !ReadString(In, Slot.DiffuseTexturePath) || !ReadString(In, Slot.NormalTexturePath)) {
			return false;
		}
		Data.MaterialSlots.Add(std::move(Slot));
	}

	// 3. AABB
	return ReadPod<FBox>(In, Data.AABB);
}

void FStaticMeshBake::WriteMeshBody(std::ofstream& Out, const FStaticMeshData& Data)
{
	// 1. 배열 4개
	WriteArray<FVertex>(Out, Data.Vertices);
	WriteArray<FLODSourceVertex>(Out, Data.LODSourceVertices);
	WriteArray<uint32>(Out, Data.Indices);
	WriteArray<FStaticMeshSection>(Out, Data.Sections);

	// 2. 머티리얼 슬롯
	WritePod<uint32>(Out, Data.MaterialSlots.Num());
	for (const auto& Slot : Data.MaterialSlots)
	{
		WriteString(Out, Slot.Name);
		WritePod<FVector4>(Out, Slot.BaseColor);
		WriteString(Out, Slot.DiffuseTexturePath);
		WriteString(Out, Slot.NormalTexturePath);
	}

	// 3. AABB
	WritePod<FBox>(Out, Data.AABB);
}
```

그리고 기존 두 함수의 해당 부분을 호출로 바꾼다.

```cpp
// ReadBaked: 헤더 검사 다음 ~ AABB 읽기까지를 →
	if (!ReadMeshBody(In, *Data))
	{
		return nullptr;
	}
	// (그 아래 Validate·로그·return은 그대로)

// WriteBaked: WritePod<FBakeHeader>(...) 다음 ~ WritePod<FBox>(...)까지를 →
	WriteMeshBody(Out, Data);
	// (그 아래 스트림 확인은 그대로)
```

- `ReadBaked` 안의 `uint32 MaxCount = 1000000;` 줄은 이제 안 쓰므로 지운다.

#### D-5-2. SkeletalMesh Bake 본문

**파일:** `Source/Runtime/Asset/SkeletalMesh/SkeletalMeshBake.cpp` — 수정

(1) include 추가: `#include "Core/EngineLog.h"` 아래

```cpp
#include "Asset/ObjImporter/StaticMeshBake.h"
```

(2) `BakeVersion{ 1 }` → `BakeVersion{ 2 }`

(3) `ReadBaked`의 shortcut 줄부터 `return nullptr;`까지를 아래로 바꾼다.

```cpp
	// 2. 메시 본문 (StaticMesh와 같은 형식)
	TUniquePtr<FSkeletalMeshData> Data = MakeUnique<FSkeletalMeshData>();
	if (!FStaticMeshBake::ReadMeshBody(In, Data->Mesh))
	{
		return nullptr;
	}

	// 3. 가중치 (숫자만 있는 구조체라 배열 통째로)
	if (!ReadArray<FSkinWeight>(In, Data->SkinWeights, 1000000))
	{
		return nullptr;
	}

	// 4. 뼈: 개수 + (이름, 부모, InverseBindPose)
	uint32 BoneCount = 0;
	if (!ReadPod(In, BoneCount) || BoneCount > 4096)
	{
		return nullptr;
	}
	for (uint32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		FBoneInfo Bone;
		if (!ReadString(In, Bone.Name) || !ReadPod(In, Bone.ParentIndex) || !ReadPod(In, Bone.InverseBindPose.M))
		{
			return nullptr;
		}
		Data->Bones.Add(std::move(Bone));
	}

	// 5. 내용 검사 (바이트 수는 맞아도 깨졌을 수 있음)
	FString Error;
	if (!Data->Validate(Error))
	{
		HTR_LOG(Warning, "[BAKE] {}: {}, re-importing", BinPath, Error);
		return nullptr;
	}

	HTR_LOG(Info, "[BAKE] {}: loaded", BinPath);
	return Data;
```

(4) `WriteBaked`의 `// shorcut` 줄을 아래로 바꾼다.

```cpp
	// 2. 메시 본문
	FStaticMeshBake::WriteMeshBody(Out, Data.Mesh);

	// 3. 가중치
	WriteArray<FSkinWeight>(Out, Data.SkinWeights);

	// 4. 뼈
	WritePod(Out, static_cast<uint32>(Data.Bones.Num()));
	for (const FBoneInfo& Bone : Data.Bones)
	{
		WriteString(Out, Bone.Name);
		WritePod(Out, Bone.ParentIndex);
		WritePod(Out, Bone.InverseBindPose.M);   // FMatrix는 복사 대입이 직접 정의돼 있어 float[4][4]로 쓴다
	}
```

- `Bone.InverseBindPose.M`으로 쓰는 이유: `FMatrix`는 `operator=`를 직접 정의해서 `WritePod`의 `is_trivially_copyable` 검사를 통과하지 못한다. 안쪽의 `float M[4][4]`는 통과한다.
- 뼈 개수 상한 4096: 깨진 파일이 거대한 개수를 적어 메모리를 터뜨리는 것을 막는다(`ReadString`의 4096 상한과 같은 이유).

### 작업 D-6. 등록 → 화면 확인

**파일:** `Source/Runtime/Asset/AssetManager.cpp` — `LoadFbxMesh`의 "3. 만들기·등록"을 바꾼다

```cpp
	// 3. 만들기·등록
	// ▼ 바꿈 (D)
	UStaticMesh* Mesh = CreateStaticMesh(&Data->Mesh);
	if (!Mesh)
	{
		return nullptr;
	}
	Get().RegisterAsset(Key, Mesh);
	return Mesh;
	// ▲ 바꿈 (D)
}
```

### D 확인

| # | 할 일 | 기대 결과 |
|---|---|---|
| 1 | 에디터 실행 | `[FBX] ...: vertices N, indices M, ...` → `[BAKE] ...fbx.bin` 저장 |
| 2 | 에디터에서 StaticMeshActor에 FBX 메시 지정 | **T-Pose 캐릭터가 바른 방향(앞이 +X, 위가 +Z)·텍스처로 보임** |
| 3 | 에디터 재실행 | `[BAKE] ...fbx.bin: loaded` (다시 굽지 않음) |
| 4 | HitoriGame 실행 | 같은 메시가 보임 (`.bin`만 읽음) |

- 누워 있으면 → `MakeAxisConversion`의 분기(파일 축 로그와 비교). 안이 보이면(뒤집힌 면) → 감김 순서. 텍스처가 흰색이면 → `texture not found` 로그.

---

## 5. E단계 — 뼈대

### 생각의 흐름

```
E-1. 노드 트리에서 뼈만 골라낸다           → Skeleton 꼬리표 or Cluster가 가리키는 노드
E-2. 부모가 먼저 오게                       → 트리 전위 순회 순서 그대로 넣기
E-3. F·G가 "이 노드는 몇 번 뼈?"를 물어봄  → FbxNode* → 인덱스 표
```

### 작업 E-1. 뼈 수집 함수

**파일:** `FbxImporter.cpp` — 익명 네임스페이스, D의 `FinishMesh` 아래

```cpp
	// ▼ 추가 (E)
	// ===================== E. 뼈 =====================

	// 뼈 수집 (전위 순회: 부모를 먼저 넣으므로 규칙 2가 저절로 지켜진다)
	// - 뼈 판정: Skeleton 꼬리표, 또는 Cluster가 가리키는 노드 (Null 노드를 뼈로 쓰는 파일 대비)
	void CollectBones(FbxNode* Node, int32 ParentBone, const std::unordered_set<FString>& ClusterBones,
		TArray<FBoneInfo>& OutBones, std::unordered_map<FbxNode*, int32>& OutBoneIndexByNode)
	{
		int32 ThisBone = ParentBone;

		// [if] 뼈 노드면 배열에 추가
		// - 이후: 이 노드의 자식들은 ThisBone을 부모로 받는다
		if (Node->GetSkeleton() || ClusterBones.contains(Node->GetName()))
		{
			ThisBone = OutBones.Num();

			FBoneInfo Bone;
			Bone.Name = Node->GetName();
			Bone.ParentIndex = ParentBone;
			OutBones.Add(Bone);
			OutBoneIndexByNode[Node] = ThisBone;
		}

		// [for] 자식으로 재귀 (뼈가 아닌 노드를 건너도 부모 뼈 번호는 이어진다)
		for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
		{
			CollectBones(Node->GetChild(ChildIndex), ThisBone, ClusterBones, OutBones, OutBoneIndexByNode);
		}
	}

	// 뼈 계층 출력 (Bone Hierarchy Tree UI의 원본)
	void PrintBones(const TArray<FBoneInfo>& Bones)
	{
		// [for] 뼈마다 깊이를 부모 사슬로 계산해 들여쓰기
		for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
		{
			int32 Depth = 0;
			for (int32 Parent = Bones[BoneIndex].ParentIndex; Parent >= 0; Parent = Bones[Parent].ParentIndex)
			{
				++Depth;
			}
			HTR_LOG(Info, "[FBX]   {}{} ({})", FString(Depth * 2, ' '), Bones[BoneIndex].Name, BoneIndex);
		}
	}
	// ▲ 추가 (E)
```

### 작업 E-2. Import에 연결

**위치:** `Import`의 "8. 축 변환 행렬" 바로 아래

```cpp
	// 8. 축 변환 행렬 (D-1)
	const FMatrix Axis = MakeAxisConversion(Scene, FbxPath);

	// ▼ 추가 (E)
	// 8-1. 뼈 배열 (E)
	std::unordered_map<FbxNode*, int32> BoneIndexByNode;
	CollectBones(Scene->GetRootNode(), -1, ClusterBones, OutData.Bones, BoneIndexByNode);
	PrintBones(OutData.Bones);
	// ▲ 추가 (E)
```

- `SkeletalMeshBake.cpp`: `BakeVersion{ 2 }` → `{ 3 }`

### E 확인

- 로그에 뼈 계층이 들여쓰기로 찍힌다. C단계의 노드 트리 중 Skeleton 줄과 순서·깊이가 같아야 한다.
- `Validate`의 "bone parent must come before child"가 걸리지 않는다.
- 화면은 D와 같다 (아직 등록은 StaticMesh).

---

## 6. F단계 — 접착: 가중치

### 생각의 흐름

```
F-1. FBX는 뼈 중심: Cluster(뼈) → (ControlPoint, 비율) 목록
F-2. 엔진은 정점 중심: 정점 → (뼈, 비율) 최대 12개
F-3. 그 사이를 잇는 다리 = D에서 남긴 Sources(정점 → ControlPoint)
```

```
Cluster "LowerArm" : CP 120 → 0.5, CP 121 → 1.0 ...      (뼈 중심)
         │  뒤집기 (ControlPoint마다 모으기)
         ▼
CP 120 : [(UpperArm, 0.5), (LowerArm, 0.5)]               (ControlPoint 중심)
         │  Sources로 펼친 정점에 복사
         ▼
정점 #87 (CP 120에서 옴) : BoneIndex[UpperArm, LowerArm], Weight[0.5, 0.5]
```

### 작업 F-1. 함수들

**파일:** `FbxImporter.cpp` — 익명 네임스페이스, E의 `PrintBones` 아래

```cpp
	// ▼ 추가 (F)
	// ===================== F. 가중치 =====================

	// 메시의 모든 Cluster 방문 (Mesh → Skin → Cluster). F·G가 같이 쓴다
	template <typename FFunc>
	void ForEachCluster(FbxMesh* Mesh, FFunc&& Func)
	{
		// [for] Skin → Cluster 이중 순회 (C단계 CollectClusterBones와 같은 길)
		const int32 SkinCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
		for (int32 SkinIndex = 0; SkinIndex < SkinCount; ++SkinIndex)
		{
			FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(SkinIndex, FbxDeformer::eSkin));
			for (int32 ClusterIndex = 0; ClusterIndex < Skin->GetClusterCount(); ++ClusterIndex)
			{
				Func(Skin->GetCluster(ClusterIndex));
			}
		}
	}

	using FInfluences = std::vector<std::pair<int32, float>>;   // (뼈 번호, 비율)

	// 영향 목록 → FSkinWeight (큰 순, 최대 12개, 합 = 1)
	FSkinWeight MakeSkinWeight(FInfluences Influences)
	{
		FSkinWeight Out;

		// 1. 큰 순 정렬
		std::sort(Influences.begin(), Influences.end(),
			[](const auto& A, const auto& B) { return A.second > B.second; });

		// 2. 최대 MAX_TOTAL_INFLUENCES개
		if (Influences.size() > MAX_TOTAL_INFLUENCES)
		{
			Influences.resize(MAX_TOTAL_INFLUENCES);
		}

		// 3. 합 계산
		float Sum = 0.0f;
		for (const auto& Influence : Influences)
		{
			Sum += Influence.second;
		}

		// [if] 영향이 하나도 없는 정점 → 0번 뼈(루트)에 고정
		// - 이유: 합이 0이면 스키닝 결과가 원점으로 빨려 들어간다
		if (Sum <= 0.0f)
		{
			Out.Weight[0] = 1.0f;
			return Out;
		}

		// 4. 재정규화 (잘라낸 만큼 합이 1보다 작아졌으므로)
		for (size_t Slot = 0; Slot < Influences.size(); ++Slot)
		{
			Out.BoneIndex[Slot] = Influences[Slot].first;
			Out.Weight[Slot] = Influences[Slot].second / Sum;
		}
		return Out;
	}

	// 가중치 채우기: 뼈 중심 → 정점 중심
	void FillSkinWeights(const std::vector<FbxNode*>& MeshNodes, const std::vector<FVertexSource>& Sources,
		const std::unordered_map<FbxNode*, int32>& BoneIndexByNode, TArray<FSkinWeight>& OutWeights, const FString& FbxPath)
	{
		// 1. 메시마다 ControlPoint별 영향 목록 (뒤집기)
		std::unordered_map<FbxMesh*, std::vector<FInfluences>> InfluencesByMesh;

		// [for] 메시 노드마다
		for (FbxNode* Node : MeshNodes)
		{
			FbxMesh* Mesh = Node->GetMesh();
			std::vector<FInfluences>& PerControlPoint = InfluencesByMesh[Mesh];
			PerControlPoint.resize(Mesh->GetControlPointsCount());

			// [for] Cluster마다 (= 뼈마다)
			ForEachCluster(Mesh, [&](FbxCluster* Cluster)
			{
				// [if] 우리 뼈 배열에 있는 뼈만
				const auto Bone = BoneIndexByNode.find(Cluster->GetLink());
				if (Bone == BoneIndexByNode.end())
				{
					return;
				}

				// 이 뼈가 움직이는 ControlPoint 번호와 비율 (병렬 배열)
				const int32 Count = Cluster->GetControlPointIndicesCount();
				const int* ControlPoints = Cluster->GetControlPointIndices();
				const double* Weights = Cluster->GetControlPointWeights();

				// [for] 뼈 중심 목록을 ControlPoint 쪽으로 옮겨 담기
				for (int32 i = 0; i < Count; ++i)
				{
					// [if] 비율 0, 범위 밖 번호는 버림
					if (Weights[i] > 0.0 && ControlPoints[i] >= 0 && ControlPoints[i] < static_cast<int32>(PerControlPoint.size()))
					{
						PerControlPoint[ControlPoints[i]].push_back({ Bone->second, static_cast<float>(Weights[i]) });
					}
				}
			});
		}

		// 2. 펼친 정점마다 자기 ControlPoint의 목록 복사 (Sources = D에서 남긴 다리)
		int32 Unweighted = 0;

		// [for] 정점 순서 = OutMesh.Vertices 순서 (규칙 1)
		for (const FVertexSource& Source : Sources)
		{
			const FInfluences& Influences = InfluencesByMesh[Source.Mesh][Source.ControlPoint];
			if (Influences.empty())
			{
				++Unweighted;
			}
			OutWeights.Add(MakeSkinWeight(Influences));
		}

		// [if] 가중치 없는 정점 보고
		if (Unweighted > 0)
		{
			HTR_LOG(Warning, "[FBX] {}: {} vertices have no weights, bound to root", FbxPath, Unweighted);
		}
	}
	// ▲ 추가 (F)
```

### 작업 F-2. Import에 연결

**위치:** `Import`의 `FinishMesh(SlotTriangles, OutData.Mesh);` 바로 아래

```cpp
	FinishMesh(SlotTriangles, OutData.Mesh);

	// ▼ 추가 (F)
	// 9-1. 가중치 (F): 뼈가 있는 파일만
	if (!OutData.Bones.IsEmpty())
	{
		FillSkinWeights(MeshNodes, Sources, BoneIndexByNode, OutData.SkinWeights, FbxPath);
	}
	// ▲ 추가 (F)
```

- `SkeletalMeshBake.cpp`: `BakeVersion{ 3 }` → `{ 4 }`

### F 확인

- `Validate`가 통과한다(개수 일치, 뼈 번호 범위, 합≈1). 실패하면 Import가 `false` + 이유 로그.
- `vertices have no weights` 경고가 없거나 아주 적다.
- (선택) 팔꿈치 근처 정점 하나의 `SkinWeights`를 로그로 찍어 두 뼈에 나뉘어 있는지 본다.

---

## 7. G단계 — 붙인 순간: 바인드 포즈

### 생각의 흐름

```
G-1. 모든 뼈에 "붙인 순간 전역 행렬"이 필요 → 기본값: 0프레임 전역 (A-8)
G-2. Cluster가 있는 뼈는 정확한 기록이 있다 → TransformLinkMatrix로 덮어쓰기
G-3. 엔진 공간으로                           → ConvertMatrix (정점과 같은 C)
G-4. 스키닝에 쓰는 건 역행렬                  → InverseBindPose
```

### 작업 G-1. 함수들

**파일:** `FbxImporter.cpp` — 익명 네임스페이스, F의 `FillSkinWeights` 아래

```cpp
	// ▼ 추가 (G)
	// ===================== G. 바인드 포즈 =====================

	// 뼈마다 InverseBindPose 채우기
	void FillBindPoses(const std::vector<FbxNode*>& MeshNodes, const std::unordered_map<FbxNode*, int32>& BoneIndexByNode,
		const FMatrix& Axis, TArray<FBoneInfo>& Bones)
	{
		std::vector<FbxAMatrix> BindGlobal(Bones.Num());

		// 1. 기본값: 0프레임 전역 행렬 (Cluster 없는 뼈 대비, A-8)
		for (const auto& [Node, BoneIndex] : BoneIndexByNode)
		{
			BindGlobal[BoneIndex] = Node->EvaluateGlobalTransform();
		}

		// 2. Cluster 기록으로 덮어쓰기
		// [for] 메시마다 → [for] Cluster마다
		for (FbxNode* Node : MeshNodes)
		{
			ForEachCluster(Node->GetMesh(), [&](FbxCluster* Cluster)
			{
				// [if] 우리 뼈 배열에 있는 뼈만
				const auto Bone = BoneIndexByNode.find(Cluster->GetLink());
				if (Bone != BoneIndexByNode.end())
				{
					Cluster->GetTransformLinkMatrix(BindGlobal[Bone->second]);   // 피부를 붙인 순간 뼈의 전역 행렬
				}
			});
		}

		// 3. 엔진 공간 → 역행렬
		// [for] 뼈마다
		for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
		{
			const FMatrix Bind = ConvertMatrix(BindGlobal[BoneIndex], Axis);
			Bones[BoneIndex].InverseBindPose = Bind.Inverse();
		}
	}

	// 검사: 뼈 위치가 메시 범위(+여유 25%) 안에 있는가
	// - 정점(D)과 뼈(G)가 같은 공간·같은 축 변환을 받았는지 확인
	// - 축이나 A-5(메시 바인드 행렬)가 틀리면 뼈가 메시 밖 엉뚱한 곳에 찍힌다
	int32 CountBonesOutsideMesh(const FSkeletalMeshData& Data)
	{
		const FBox& Box = Data.Mesh.AABB;
		const FVector Margin = (Box.Max - Box.Min) * 0.25f;
		int32 Outside = 0;

		// [for] 뼈마다 바인드 위치 = InverseBindPose의 역행렬의 이동 성분
		for (const FBoneInfo& Bone : Data.Bones)
		{
			FMatrix Bind = Bone.InverseBindPose.Inverse();
			const FVector4 Origin = Bind.GetOrigin();

			// [if] 범위 밖
			if (Origin.X < Box.Min.X - Margin.X || Origin.X > Box.Max.X + Margin.X ||
				Origin.Y < Box.Min.Y - Margin.Y || Origin.Y > Box.Max.Y + Margin.Y ||
				Origin.Z < Box.Min.Z - Margin.Z || Origin.Z > Box.Max.Z + Margin.Z)
			{
				++Outside;
			}
		}
		return Outside;
	}
	// ▲ 추가 (G)
```

### 작업 G-2. Import에 연결

**위치:** F에서 넣은 가중치 블록 바로 아래, "10. 검사" 위

```cpp
	// ▼ 추가 (G)
	// 9-2. 바인드 포즈 (G)
	FillBindPoses(MeshNodes, BoneIndexByNode, Axis, OutData.Bones);

	// [if] 뼈가 메시 밖에 있으면 경고 (정점과 뼈의 공간이 어긋남)
	if (const int32 Outside = CountBonesOutsideMesh(OutData); Outside > 0)
	{
		HTR_LOG(Warning, "[FBX] {}: {} bones are outside the mesh bounds (axis or bind matrix mismatch?)", FbxPath, Outside);
	}
	// ▲ 추가 (G)
```

- `SkeletalMeshBake.cpp`: `BakeVersion{ 4 }` → `{ 5 }`

### G 확인

- `bones are outside the mesh bounds` 경고가 **없다**.
- 있으면: D가 화면에서 바르게 보였다면 축은 맞으므로, A-5(`GetMeshBindMatrix`)나 A-8 대체 값(0프레임 자세가 T-Pose가 아닌 파일)을 의심한다.

> **ADR 정정**: ADR에 G의 확인을 "T-Pose에서 `v · InverseBindPose · BindPose == v`"로 적었는데, 이것은 어떤 행렬이든 항상 참이라 아무것도 검사하지 못한다. 대신 위의 "뼈 위치가 메시 안에 있는가"로 바꿨다. 진짜 검증은 뷰어에서 뼈를 메시 위에 그려 볼 때(Viewer 단계) 완성된다.

---

## 8. H단계 — 마무리: 등록 분기, 저장 검사

### 생각의 흐름

```
H-1. 뼈가 있으면 SkeletalMesh, 없으면 StaticMesh   (UE도 Skin 유무로 나눔)
H-2. SkeletalMesh = CPU 원본 데이터 + 바인드 포즈 렌더 메시
H-3. 저장한 .bin을 바로 다시 읽어 개수 비교
```

> **주의**: H를 하면 뼈가 있는 FBX는 `UStaticMesh`가 아니라 `USkeletalMesh`로 등록된다. 그러면 StaticMeshActor에서 더 이상 고를 수 없다. `USkeletalMeshComponent`를 만들 때 H를 같이 하는 것을 추천한다. 그 전까지는 D~G 상태로 두면 화면 확인을 계속할 수 있다.

### 작업 H-1. `USkeletalMesh`

**파일:** `Source/Runtime/Render/SkeletalMesh.h` — **새 파일** (`Mesh.h`의 `UStaticMesh` 옆)

```cpp
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
```

### 작업 H-2. 등록 분기

**파일:** `Source/Runtime/Asset/AssetManager.h` — `LoadFbxMesh` 반환형

```cpp
	static URenderAsset* LoadFbxMesh(const FString& Path);   // UStaticMesh* → URenderAsset*
```

**파일:** `Source/Runtime/Asset/AssetManager.cpp`

(1) include: `#include "Render/SkeletalMeshData.h"` 아래에 `#include "Render/SkeletalMesh.h"`

(2) `LoadFbxMesh` 전체

```cpp
URenderAsset* UAssetManager::LoadFbxMesh(const FString& Path)
{
	// 1. 키 확인 (Static이든 Skeletal이든)
	const FString Key = MakeAssetKey(Path);
	if (URenderAsset* Cached = GetAssetByPath<URenderAsset>(Key))
	{
		return Cached;
	}

	// 2. 데이터 얻기
	TUniquePtr<FSkeletalMeshData> Data = LoadSkeletalMeshData(Path, SkeletalMeshImporter);
	if (!Data)
	{
		return nullptr;
	}

	// 3. 바인드 포즈 메시 (GPU 버퍼·머티리얼). Static/Skeletal 공통
	UStaticMesh* RenderMesh = CreateStaticMesh(&Data->Mesh);
	if (!RenderMesh)
	{
		return nullptr;
	}

	// [if] 뼈 없음 → StaticMesh로 등록 (FBX StaticMesh 임포트)
	if (Data->Bones.IsEmpty())
	{
		Get().RegisterAsset(Key, RenderMesh);
		return RenderMesh;
	}

	// 4. 뼈 있음 → SkeletalMesh로 등록
	USkeletalMesh* SkeletalMesh = NewObject<USkeletalMesh>();
	SkeletalMesh->Data = std::move(*Data);
	SkeletalMesh->RenderMesh = RenderMesh;
	Get().RegisterAsset(Key, SkeletalMesh);
	return SkeletalMesh;
}
```

### 작업 H-3. 저장 직후 검사

**파일:** `AssetManager.cpp` — `LoadSkeletalMeshData`의 "5. 캐시 저장" 부분

```cpp
		// 5. 캐시 저장
		FSkeletalMeshBake::WriteBaked(BinPath, *Data);

		// ▼ 추가 (H)
		// 6. self-check: 저장한 것을 다시 읽어 개수 비교 (직렬화 오류를 그 자리에서 발견)
		const TUniquePtr<FSkeletalMeshData> Reloaded = FSkeletalMeshBake::ReadBaked(BinPath);
		if (!Reloaded ||
			Reloaded->Mesh.Vertices.Num() != Data->Mesh.Vertices.Num() ||
			Reloaded->SkinWeights.Num() != Data->SkinWeights.Num() ||
			Reloaded->Bones.Num() != Data->Bones.Num())
		{
			HTR_LOG(Error, "[BAKE] {}: reload mismatch", BinPath);
		}
		// ▲ 추가 (H)
		return Data;
```

### H 확인

- 새 파일 `SkeletalMesh.h` → `SlnGenerator.bat` 재실행(헤더 툴이 `SkeletalMesh.generated.h`를 만든다).
- 에디터 실행: `reload mismatch`가 없다.
- 뼈 없는 FBX(소품)는 여전히 StaticMeshActor에서 보인다. 뼈 있는 FBX는 `USkeletalMesh`로 등록된다(컴포넌트 단계에서 사용).

---

## 9. 막히면

| 증상 | 의심할 곳 |
|---|---|
| 메시가 누워 있음 / 옆을 봄 | `MakeAxisConversion` 분기. C단계 `Axis:` 로그와 비교 |
| 메시 안쪽이 보임(면이 뒤집힘) | `AppendMesh` 7번 감김 순서 `(0, 2, 1)` |
| 텍스처가 흰색 | `texture not found` 로그 → 파일 위치, `.fbm` 폴더 |
| 메시가 조각나 보임 | 메시가 여러 개인데 `Bind` 행렬이 메시마다 다른가 (`GetMeshBindMatrix`) |
| 너무 크거나 작음 | A-7: 단위 변환 안 함. 액터 Scale로 조정 |
| `weights do not sum to 1` | `MakeSkinWeight` 재정규화 / `Sum <= 0` 처리 |
| `bone index out of range` | `CollectBones`가 Cluster 뼈를 빠뜨림 (`ClusterBones` 판정) |
| `bones are outside the mesh bounds` | `GetMeshBindMatrix`(A-5), 0프레임이 T-Pose가 아닌 파일(A-8) |
| 고쳤는데 결과가 그대로 | `BakeVersion`을 안 올려서 옛 `.bin`을 읽는 중 |
| `FMatrix` 관련 `WritePod` 컴파일 오류 | `.M`(float 배열)으로 쓰는지 |
| `SkeletalMesh.generated.h` 없음 | `SlnGenerator.bat` 재실행 후 빌드(헤더 툴이 생성) |

---

## 부록. `FbxImporter.cpp` 완성본 (H까지)

C단계 문서의 함수(`FFbxManagerDeleter`, `GetAttributeName`, `CollectClusterBones`, `PrintNode`, `PrintSceneInfo`)는 그대로이고, 그 아래에 4~7장의 D·E·F·G 함수가 순서대로 붙는다. `Import`의 최종 모습은 아래와 같다.

```cpp
bool FFbxImporter::Import(const FString& FbxPath, FSkeletalMeshData& OutData)
{
	// 1. SDK 준비: Manager 생성 + 읽기 옵션(IOSettings) 연결
	FFbxManagerPtr Manager(FbxManager::Create());
	Manager->SetIOSettings(FbxIOSettings::Create(Manager.get(), IOSROOT));

	// 2. 파일 열기
	FbxImporter* Importer = FbxImporter::Create(Manager.get(), "");
	if (!Importer->Initialize(FbxPath.c_str(), -1, Manager->GetIOSettings()))
	{
		HTR_LOG(Error, "[FBX] {}: open failed ({})", FbxPath, Importer->GetStatus().GetErrorString());
		return false;
	}

	// 3. 파일 → Scene
	FbxScene* Scene = FbxScene::Create(Manager.get(), "ImportScene");
	if (!Importer->Import(Scene))
	{
		HTR_LOG(Error, "[FBX] {}: import failed ({})", FbxPath, Importer->GetStatus().GetErrorString());
		return false;
	}

	// 4. 파일 핸들 조기 해제
	Importer->Destroy();

	// 5. 구조 출력 (C)
	HTR_LOG(Info, "[FBX] ===== {} =====", FbxPath);
	PrintSceneInfo(Scene);

	// 6. Cluster 뼈 수집 → 노드 트리 출력 (C)
	std::unordered_set<FString> ClusterBones;
	CollectClusterBones(Scene->GetRootNode(), ClusterBones);
	PrintNode(Scene->GetRootNode(), 0, ClusterBones);

	// 7. 삼각형화 (D)
	FbxGeometryConverter Converter(Manager.get());
	if (!Converter.Triangulate(Scene, true))
	{
		HTR_LOG(Warning, "[FBX] {}: triangulate failed on some meshes", FbxPath);
	}

	// 8. 축 변환 행렬 (D)
	const FMatrix Axis = MakeAxisConversion(Scene, FbxPath);

	// 8-1. 뼈 배열 (E)
	std::unordered_map<FbxNode*, int32> BoneIndexByNode;
	CollectBones(Scene->GetRootNode(), -1, ClusterBones, OutData.Bones, BoneIndexByNode);
	PrintBones(OutData.Bones);

	// 9. 메시 (D)
	std::vector<FbxNode*> MeshNodes;
	CollectMeshNodes(Scene->GetRootNode(), MeshNodes);

	const bool bAnySkin = !ClusterBones.empty();
	std::vector<std::vector<uint32>> SlotTriangles;
	std::vector<FVertexSource> Sources;
	for (FbxNode* Node : MeshNodes)
	{
		if (bAnySkin && Node->GetMesh()->GetDeformerCount(FbxDeformer::eSkin) == 0)
		{
			HTR_LOG(Warning, "[FBX] {}: mesh '{}' has no skin, skipped", FbxPath, Node->GetName());
			continue;
		}
		AppendMesh(Node, FbxPath, Axis, OutData.Mesh, SlotTriangles, Sources);
	}
	FinishMesh(SlotTriangles, OutData.Mesh);

	// 9-1. 가중치 (F)
	if (!OutData.Bones.IsEmpty())
	{
		FillSkinWeights(MeshNodes, Sources, BoneIndexByNode, OutData.SkinWeights, FbxPath);
	}

	// 9-2. 바인드 포즈 (G)
	FillBindPoses(MeshNodes, BoneIndexByNode, Axis, OutData.Bones);
	if (const int32 Outside = CountBonesOutsideMesh(OutData); Outside > 0)
	{
		HTR_LOG(Warning, "[FBX] {}: {} bones are outside the mesh bounds (axis or bind matrix mismatch?)", FbxPath, Outside);
	}

	// 10. 검사
	FString Error;
	if (!OutData.Validate(Error))
	{
		HTR_LOG(Error, "[FBX] {}: {}", FbxPath, Error);
		return false;
	}

	HTR_LOG(Info, "[FBX] {}: vertices {}, indices {}, sections {}, materials {}, bones {}", FbxPath,
		OutData.Mesh.Vertices.Num(), OutData.Mesh.Indices.Num(), OutData.Mesh.Sections.Num(),
		OutData.Mesh.MaterialSlots.Num(), OutData.Bones.Num());
	return true;

	// 11. 정리: 스코프 종료 시 Manager Destroy
}
```

# FBX Importer C단계 — 굽는 사람 만들기: FBX SDK 연결하고 트리 보기 (따라 하기)

상위 문서: [ADR-0001 FBX Importer 이식](0001-fbx-importer.md) · 이전 단계: [B단계](0001-fbx-importer-B.md)

## 이 문서 읽는 법

- **1~2장**: 왜 이렇게 만드는지(생각의 흐름).
- **3장(작업 0~5)**: 따라 하기. 각 작업은 **파일 → 위치 → 코드** 순서다.
  - 코드 블록 안의 `// ▼ 추가` ~ `// ▲ 추가`(premake는 `-- ▼ 추가`) 사이만 새로 쓰고, 그 바깥 줄은 위치를 찾기 위한 기존 코드다.
  - **새 파일**은 블록 전체를 그대로 만든다.
- **4장**: 빌드와 확인. **5장**: 출력에서 무엇을 읽어야 하나(D단계 준비).

---

## 1. 이 단계가 끝나면

- 에디터를 켜면 `Assets` 안의 `.fbx`마다 Output Log에 **씬 정보와 노드 트리**가 찍힌다.
- 이어서 `[FBX] ...: mesh data not implemented yet` 경고가 나온다. 아직 `FSkeletalMeshData`를 채우지 않으니 정상이다(`.bin`도 만들지 않는다).
- 게임(HitoriGame)은 B단계와 똑같이 동작하고, **FBX SDK를 링크하지 않는다.**

---

## 2. 생각의 흐름

B단계에서 길은 다 깔았다. 남은 빈칸은 하나다.

```
UEditorEngine::RegisterAssetImporters
 └ SetSkeletalMeshImporter( 가짜 람다 )   ← 이 자리를 진짜 굽는 사람으로 바꾼다
```

```
생각 1. 굽는 사람이 FBX를 읽으려면?   → SDK를 엔진에 붙인다. 어디에? 에디터에만      (premake)
생각 2. 굽는 사람은 어떤 모양?        → B에서 이미 정했다: bool(경로, FSkeletalMeshData&)  (FbxImporter.h)
생각 3. 파일을 어떻게 여나?           → Manager → Importer → Scene, 다 쓰면 Manager째 정리 (FbxImporter.cpp)
생각 4. 연 것을 어떻게 보나?          → 노드 트리 + D단계 전에 알아야 할 3가지를 찍는다  (FbxImporter.cpp)
생각 5. 가짜를 진짜로                 → EditorEngine 한 줄                                (EditorEngine.cpp)
```

### UE와의 대응

| 우리 | UE (레거시 FBX 임포터) | 하는 일 |
|---|---|---|
| `FFbxImporter::Import` | `UFbxFactory` → `UnFbx::FFbxImporter::ImportFromFile` | 굽는 사람의 입구 |
| Manager·IOSettings 생성 | `FFbxImporter` 생성자 | SDK 준비 |
| `Importer->Initialize` | `FFbxImporter::OpenFile` | 파일 열기 |
| `Importer->Import(Scene)` | `FFbxImporter::ImportFile` | 파일 → `FbxScene` |
| (D단계) 축 변환 | `FFbxImporter::ConvertScene` | 좌표계 맞추기 |
| `Editor/Fbx/` 에만 SDK | `UnrealEd` 모듈에만 SDK 링크 | 게임은 SDK를 모른다 |

---

## 3. 따라 하기

### 작업 0. 준비 — FBX SDK 설치 확인

1. Autodesk FBX SDK(**2020.3.x, Windows, VS2022용**)를 설치한다. 기본 위치는 아래와 같다.
   `C:\Program Files\Autodesk\FBX\FBX SDK\<버전>\`
2. 그 폴더 안에 아래 3가지가 있는지 확인한다. 이 경로가 작업 1에 그대로 들어간다.
   - `include\fbxsdk.h`
   - `lib\x64\debug\libfbxsdk-md.lib` (+ `libxml2-md.lib`, `zlib-md.lib`)
   - `lib\x64\release\libfbxsdk-md.lib` (+ 같은 2개)
   - 설치본에 따라 `lib\vs2022\x64\...`처럼 VS 이름 폴더가 하나 더 있을 수 있다. 실제 폴더 이름으로 작업 1을 맞춘다.
3. 새 폴더를 하나 만든다: `Source/Editor/Fbx/`

**왜 `-md`인가**: 우리 엔진은 `staticruntime "off"`, 즉 /MD(디버그는 /MDd)로 빌드한다. FBX SDK의 정적 lib 중 이 런타임과 맞는 것이 `-md` 계열이다. `-mt`를 섞으면 링크 오류(LNK2038 런타임 불일치)가 난다.

**왜 SDK를 리포에 넣지 않나**: lib가 수백 MB라 리포가 무거워진다. 그래서 각자 설치하고, premake가 설치 경로를 가리키게 한다. 경로가 다르면 환경 변수 `FBXSDK_DIR`로 바꿀 수 있게 해 둔다.

---

### 작업 1. 생각 1 — SDK를 에디터에만 붙인다

**파일:** `premake5.lua` — 수정

#### 1-1. SDK 위치 변수

**위치:** 파일 위쪽 `IncludeDir["miniaudio"] = ...` 줄 바로 아래

```lua
IncludeDir["miniaudio"]  = "Source/ThirdParty/miniaudio"
-- ▼ 추가
-- FBX SDK는 리포에 넣지 않고 각자 설치한다. 경로가 다르면 환경 변수 FBXSDK_DIR로 바꾼다.
-- (버전 폴더 이름은 실제 설치본에 맞춘다)
FbxSdkDir = os.getenv("FBXSDK_DIR") or "C:/Program Files/Autodesk/FBX/FBX SDK/2020.3.7"
-- ▲ 추가
```

#### 1-2. `HitoriEditor`에만 include·lib 연결

**위치:** `project "HitoriEditor"` 블록 안.
(1) `includedirs { ... }`의 마지막 줄 `"%{IncludeDir.sol2}",` 바로 아래

```lua
		"%{IncludeDir.lua}",
		"%{IncludeDir.sol2}",
		-- ▼ 추가
		FbxSdkDir .. "/include",
		-- ▲ 추가
	}
```

(2) 같은 블록의 `links { "d3d11.lib", ... }`가 끝나는 `}` 바로 아래, `filter "configurations:ObjViewer"` 위

```lua
	links
	{
		"d3d11.lib",
		"dxgi.lib",
		"dxguid.lib",
		"d3dcompiler.lib",
	}

	-- ▼ 추가
	-- FBX SDK 정적 lib. 엔진 런타임(/MD, /MDd)과 맞는 -md 계열을 쓴다.
	-- libxml2가 Windows 난수 함수(BCryptGenRandom)를 써서 bcrypt도 필요하다 (없으면 LNK2019)
	links { "libfbxsdk-md", "libxml2-md", "zlib-md", "bcrypt" }

	-- 런타임 "Debug"(/MDd)인 구성은 debug lib
	filter "configurations:Debug or Benchmark_Debug"
		libdirs { FbxSdkDir .. "/lib/x64/debug" }

	-- 나머지(Release, Shipping, ObjViewer, Benchmark)는 release lib
	filter { "configurations:not Debug", "configurations:not Benchmark_Debug" }
		libdirs { FbxSdkDir .. "/lib/x64/release" }

	filter {}
	-- ▲ 추가

	filter "configurations:ObjViewer"
		targetname "ObjViewer"
```

**왜**
- `HitoriEngine`(Runtime)과 `HitoriGame`에는 **아무것도 추가하지 않는다.** UE가 FBX SDK를 `UnrealEd`에만 링크하는 것과 같다. 게임 빌드가 SDK 없이 성공하는 것이 이 결정의 확인 방법이다(4장 확인 4번).
- `filter {}`로 닫는 이유: premake의 filter는 닫기 전까지 아래 줄에 계속 걸린다. 닫지 않으면 아래 `targetname` 설정이 엉뚱한 구성에만 걸린다.
- Debug 계열 판단은 `CommonSettings()`의 `runtime "Debug"`가 걸린 구성(`Debug`, `Benchmark_Debug`) 기준이다. lib의 런타임과 실행 파일의 런타임이 다르면 링크 오류가 난다.

---

### 작업 2. 생각 2 — 굽는 사람의 모양

**파일:** `Source/Editor/Fbx/FbxImporter.h` — **새 파일**

```cpp
#pragma once

#include "Core/EngineString.h"

struct FSkeletalMeshData;

// FBX 파일을 FSkeletalMeshData로 굽는다. FBX SDK를 쓰는 유일한 곳이다(에디터 전용).
// 모양은 UAssetManager::FSkeletalMeshImporter와 같다: bool(경로, 채울 데이터).
class FFbxImporter
{
public:
	static bool Import(const FString& FbxPath, FSkeletalMeshData& OutData);
};
```

**왜**
- 시그니처는 새로 정하지 않는다. B단계에서 AssetManager에 비워 둔 자리(`std::function<bool(const FString&, FSkeletalMeshData&)>`)에 **그대로 꽂히는 모양**이다.
- 이 헤더는 `fbxsdk.h`를 include하지 않는다. 그래서 이 헤더를 include하는 `EditorEngine.cpp`도 FBX SDK를 모른다. SDK 타입(`FbxNode*` 등)은 `FbxImporter.cpp` 밖으로 나가지 않는다.
- 이름 주의: 우리 클래스는 `FFbxImporter`(F 접두사), SDK의 클래스는 `FbxImporter`다. 서로 다른 이름이다.

---

### 작업 3·4. 생각 3·4 — 파일을 열고, 연 것을 본다

**파일:** `Source/Editor/Fbx/FbxImporter.cpp` — **새 파일**

```cpp
#include "EnginePCH.h"
#include "Editor/Fbx/FbxImporter.h"

#include <fbxsdk.h>          // FBX SDK 전체 (FbxManager, FbxScene, FbxNode, FbxMesh, FbxSkin ...)
#include <format>
#include <memory>
#include <unordered_set>

#include "Render/SkeletalMeshData.h"
#include "Core/EngineLog.h"

// FBX SDK 객체 관계 (이 파일이 따라가는 길)
//   FbxManager                     모든 SDK 객체의 주인. Destroy 시 전부 정리
//    ├ FbxIOSettings               읽기 옵션
//    ├ FbxImporter                 파일 → Scene 변환기 (일회용)
//    └ FbxScene                    파일 하나의 내용 전체
//       ├ FbxGlobalSettings        축·단위
//       ├ 텍스처 목록              FbxFileTexture (이미지 경로)
//       └ RootNode (FbxNode)       노드 트리의 뿌리
//          └ FbxNode ...           칸 하나 = 이름 + Transform + 자식 + 꼬리표
//             └ 꼬리표             FbxNodeAttribute (FbxMesh / FbxSkeleton / ...)
//                FbxMesh
//                 └ FbxSkin        메시에 붙은 변형기
//                    └ FbxCluster  뼈 1개당 1개. GetLink() = 가리키는 뼈 노드

namespace
{
	// Manager 자동 정리용 삭제자
	// - unique_ptr가 스코프를 벗어날 때 delete 대신 Destroy 호출
	// - Manager가 낳은 Importer·Scene·Node까지 함께 정리
	struct FFbxManagerDeleter
	{
		void operator()(FbxManager* Manager) const { Manager->Destroy(); }
	};
	using FFbxManagerPtr = std::unique_ptr<FbxManager, FFbxManagerDeleter>;

	// 노드의 꼬리표(속성) 이름
	// - 꼬리표 = 이 칸이 무엇인지 (메시·뼈·카메라 ...)
	// - 꼬리표 없음 = 위치만 있는 빈 칸 (예: RootNode)
	const char* GetAttributeName(FbxNode* Node)
	{
		// 1. 꼬리표 조회
		const FbxNodeAttribute* Attribute = Node->GetNodeAttribute();
		if (!Attribute)
		{
			return "-";
		}

		// 2. 종류별 이름
		switch (Attribute->GetAttributeType())
		{
		case FbxNodeAttribute::eMesh:     return "Mesh";       // 정점·폴리곤 (피부)
		case FbxNodeAttribute::eSkeleton: return "Skeleton";   // 뼈 (정점 없음)
		case FbxNodeAttribute::eNull:     return "Null";       // 그룹용 빈 칸 (예: Armature)
		case FbxNodeAttribute::eCamera:   return "Camera";
		case FbxNodeAttribute::eLight:    return "Light";
		default:                          return "Other";
		}
	}

	// Cluster가 가리키는 뼈 이름 수집
	// - 경로: Mesh → Skin → Cluster → Link(뼈 노드)
	// - 트리 출력 전에 먼저 수집 (Cluster는 메시 가지, 뼈는 다른 가지에 있음)
	void CollectClusterBones(FbxNode* Node, std::unordered_set<FString>& OutBones)
	{
		// 1. 메시 꼬리표 확인 (없으면 nullptr)
		if (FbxMesh* Mesh = Node->GetMesh())
		{
			// 2. 메시에 붙은 Skin 순회 (보통 0개 또는 1개)
			const int32 SkinCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
			for (int32 SkinIndex = 0; SkinIndex < SkinCount; ++SkinIndex)
			{
				FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(SkinIndex, FbxDeformer::eSkin));

				// 3. Skin의 Cluster 순회 (뼈 1개당 1개)
				for (int32 ClusterIndex = 0; ClusterIndex < Skin->GetClusterCount(); ++ClusterIndex)
				{
					// 4. Cluster가 가리키는 뼈 노드 이름 기록
					if (FbxNode* Bone = Skin->GetCluster(ClusterIndex)->GetLink())
					{
						OutBones.insert(Bone->GetName());
					}
				}
			}
		}

		// 5. 자식 노드로 재귀
		for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
		{
			CollectClusterBones(Node->GetChild(ChildIndex), OutBones);
		}
	}

	// 노드 트리 출력 (한 줄 = 노드 하나, 들여쓰기 = 깊이)
	void PrintNode(FbxNode* Node, int32 Depth, const std::unordered_set<FString>& ClusterBones)
	{
		// 1. 들여쓰기 + 이름 + 꼬리표
		FString Line(Depth * 2, ' ');
		Line += std::format("{} [{}]", Node->GetName(), GetAttributeName(Node));

		// 2. 메시 정보: ControlPoint(위치) 수, 폴리곤 수, Skin 수
		if (FbxMesh* Mesh = Node->GetMesh())
		{
			Line += std::format(" ControlPoints={} Polygons={} Skins={}",
				Mesh->GetControlPointsCount(), Mesh->GetPolygonCount(), Mesh->GetDeformerCount(FbxDeformer::eSkin));
		}

		// 3. Cluster 없는 뼈 표시 (A-8: 바인드 행렬 대체 대상)
		if (Node->GetSkeleton() && !ClusterBones.contains(Node->GetName()))
		{
			Line += " (no cluster)";
		}

		// 4. 노드 Scale 표시 (A-7: 1이 아닐 때만)
		const FbxDouble3 Scale = Node->LclScaling.Get();   // Lcl = 부모 기준 로컬 값
		if (Scale[0] != 1.0 || Scale[1] != 1.0 || Scale[2] != 1.0)
		{
			Line += std::format(" Scale=({}, {}, {})", Scale[0], Scale[1], Scale[2]);
		}

		HTR_LOG(Info, "[FBX]   {}", Line);

		// 5. 자식 노드로 재귀 (깊이 + 1)
		for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
		{
			PrintNode(Node->GetChild(ChildIndex), Depth + 1, ClusterBones);
		}
	}

	// 씬 전체 정보 출력: 축·단위(A-7, D-1), 텍스처 경로(A-10)
	void PrintSceneInfo(FbxScene* Scene)
	{
		// 1. 파일 전역 설정
		// SDK의 GetAxisSystem()은 const 함수가 아니라 Settings를 const 없이 받는다
		FbxGlobalSettings& Settings = Scene->GetGlobalSettings();
		const FbxAxisSystem Axis = Settings.GetAxisSystem();   // 값으로 돌려주므로 복사본

		// 2. 위쪽 축과 부호 (eXAxis=1, eYAxis=2, eZAxis=3)
		int UpSign = 0;
		const FbxAxisSystem::EUpVector Up = Axis.GetUpVector(UpSign);

		// 3. 오른손/왼손 좌표계
		const bool bRightHanded = Axis.GetCoorSystem() == FbxAxisSystem::eRightHanded;

		// 4. 축·단위 출력 (단위: 1 = ? cm, cm 파일이면 1, m 파일이면 100)
		HTR_LOG(Info, "[FBX] Axis: Up={}{}, {} / Unit: 1 = {} cm",
			UpSign < 0 ? "-" : "+", "?XYZ"[Up], bRightHanded ? "RightHanded" : "LeftHanded",
			Settings.GetSystemUnit().GetScaleFactor());

		// 5. 텍스처 경로 출력 (절대 경로 + .fbx 기준 상대 경로)
		for (int32 TextureIndex = 0; TextureIndex < Scene->GetTextureCount(); ++TextureIndex)
		{
			// 이미지 파일 텍스처만 (FbxCast 실패 시 nullptr)
			if (FbxFileTexture* Texture = FbxCast<FbxFileTexture>(Scene->GetTexture(TextureIndex)))
			{
				HTR_LOG(Info, "[FBX] Texture: {} (relative: {})", Texture->GetFileName(), Texture->GetRelativeFileName());
			}
		}
	}
}

bool FFbxImporter::Import(const FString& FbxPath, FSkeletalMeshData& OutData)
{
	// 1. SDK 준비: Manager 생성 + 읽기 옵션(IOSettings) 연결
	FFbxManagerPtr Manager(FbxManager::Create());
	Manager->SetIOSettings(FbxIOSettings::Create(Manager.get(), IOSROOT));

	// 2. 파일 열기: FBX 형식·버전 확인 (-1 = 형식 자동 판별)
	FbxImporter* Importer = FbxImporter::Create(Manager.get(), "");
	if (!Importer->Initialize(FbxPath.c_str(), -1, Manager->GetIOSettings()))
	{
		HTR_LOG(Error, "[FBX] {}: open failed ({})", FbxPath, Importer->GetStatus().GetErrorString());
		return false;
	}

	// 3. 파일 → Scene: 노드 트리를 메모리에 생성
	FbxScene* Scene = FbxScene::Create(Manager.get(), "ImportScene");
	if (!Importer->Import(Scene))
	{
		HTR_LOG(Error, "[FBX] {}: import failed ({})", FbxPath, Importer->GetStatus().GetErrorString());
		return false;
	}

	// 4. 파일 핸들 조기 해제 (Scene은 남음)
	Importer->Destroy();

	// 5. 씬 정보 출력
	HTR_LOG(Info, "[FBX] ===== {} =====", FbxPath);
	PrintSceneInfo(Scene);

	// 6. Cluster 뼈 수집 → 노드 트리 출력
	std::unordered_set<FString> ClusterBones;
	CollectClusterBones(Scene->GetRootNode(), ClusterBones);
	PrintNode(Scene->GetRootNode(), 0, ClusterBones);

	// 7. 결과 반환
	// shortcut: C단계는 OutData 미작성 → false 반환(.bin 미생성). D단계부터 채우고 true.
	(void)OutData;
	HTR_LOG(Warning, "[FBX] {}: mesh data not implemented yet", FbxPath);
	return false;

	// 8. 정리: 스코프 종료 시 Manager Destroy (Scene·Node 포함)
}
```

주석 규칙: 함수 머리 = 무엇을 하는지, 본문 = 번호 + 짧은 명사구(실행 순서). 파일 맨 위의 객체 관계도는 해부도 페이지의 FBX 트리와 같은 그림이다.

**왜 — 생각 3 (열기·정리)**
- FBX SDK의 객체는 전부 **Manager가 낳고 Manager가 거둔다.** `FbxImporter`, `FbxScene`, 그 안의 `FbxNode`까지 Manager가 소유한다. 그래서 정리는 `Manager->Destroy()` 한 번이면 된다.
- 그 한 번을 `unique_ptr`에 맡겼다. 중간에 실패해서 `return false`로 나가도 Manager가 정리된다. (`Importer->Destroy()`를 따로 부른 것은 파일 핸들을 일찍 닫기 위한 것이다. 안 불러도 Manager가 지운다.)
- `Initialize`(열기)와 `Import`(읽기)가 나뉘어 있다. 열기는 "FBX 파일이 맞나·버전이 읽을 수 있나"를 보고, 읽기는 실제로 `FbxScene` 트리를 만든다. UE의 `OpenFile`/`ImportFile`과 같은 구분이다.
- 실패하면 **이유를 직접 로그로 남긴다.** B단계의 약속("실패 로그는 Importer가 남긴다")이다. AssetManager는 `false`만 받고 아무 말도 하지 않는다.

**왜 — 생각 4 (보기)**
- C단계의 목적은 **FBX 자료 구조를 눈으로 보는 것**이다. 해부도에서 본 `FbxScene → FbxNode 트리 → 꼬리표(Mesh/Skeleton) → Skin → Cluster`를 실제 파일에서 확인한다.
- 그냥 트리만 찍지 않고, **A단계에서 "C에서 확인하자"고 미뤄 둔 3가지**를 같이 찍는다.

| 출력 | 무엇을 확인하나 | 관련 결정 |
|---|---|---|
| `(no cluster)` 표시 | Cluster가 없는 뼈가 실제로 있는가 | A-8 |
| `Scale=(...)` 표시, `Unit: 1 = ? cm` | 노드에 Scale이 있나, 파일 단위는 무엇인가 | A-7 |
| `Axis: Up=...` | 파일의 위쪽 축과 손 방향 | D-1 축 변환 |
| `Texture: ... (relative: ...)` | 텍스처가 외부 파일인가 내장(`.fbm` 폴더)인가 | A-10 |

- `CollectClusterBones`를 트리 출력과 따로 먼저 도는 이유: Cluster는 **메시 쪽**에 붙어 있고, 뼈 노드는 메시와 다른 가지에 있다(해부도의 "트리를 건너는 연결"). 뼈 줄을 찍을 때 "이 뼈를 가리키는 Cluster가 있나"를 알려면 먼저 메시 쪽을 한 바퀴 돌아 모아 둬야 한다.
- `false`를 돌려주는 이유: `true`를 돌려주면 아래층이 빈 데이터를 `.bin`으로 저장한다. 그러면 다음 실행부터는 이 빈 캐시를 읽는다. 데이터를 채우는 D단계 전까지는 굽기 실패로 둔다.

---

### 작업 5. 생각 5 — 가짜를 진짜로 바꿔 끼운다

**파일:** `Source/Editor/HitoriEd/EditorEngine.cpp` — 수정

(1) **include:** `#include "Render/SkeletalMeshData.h"` 줄을 아래로 **바꾼다**(람다가 없어지면 이 헤더는 필요 없다).

```cpp
#include "Asset/AssetManager.h"
// ▼ 바꿈 (이전: #include "Render/SkeletalMeshData.h")
#include "Editor/Fbx/FbxImporter.h"
// ▲ 바꿈
#include "Render/RenderResourceManager.h"
```

(2) **함수:** 파일 맨 아래의 `UEditorEngine::RegisterAssetImporters`를 통째로 바꾼다.

```cpp
// 1. FBX 임포터 등록 (스캔 전에 호출된다)
void UEditorEngine::RegisterAssetImporters()
{
	UAssetManager::SetSkeletalMeshImporter(&FFbxImporter::Import);
}
```

**왜**
- 함수 포인터 `&FFbxImporter::Import`는 `bool(const FString&, FSkeletalMeshData&)` 모양이라 `std::function` 자리에 그대로 들어간다. 이게 작업 2에서 시그니처를 B에 맞춘 이유다.
- 런타임 쪽(AssetManager, Engine, LaunchEngineLoop)은 **한 줄도 바뀌지 않는다.** B에서 깐 길 위에서 빈칸만 바꿨다.

---

## 4. 빌드와 확인

### 4-1. 프로젝트 파일 다시 만들기

1. Visual Studio를 닫는다.
2. 저장소 루트의 **`SlnGenerator.bat`을 실행**한다. 새 파일 2개(`FbxImporter.h/.cpp`)와 premake 변경(include·lib)이 반영된다.
3. `Hitori.slnx`를 열고 `HitoriEditor` → 속성 → 링커 → 입력에 `libfbxsdk-md.lib`가 있는지 본다.

### 4-2. 확인

| # | 할 일 | 기대 결과 | 무엇을 확인하나 |
|---|---|---|---|
| 1 | `HitoriEditor` **Debug** 빌드 | 성공 | debug lib 경로·`-md` 런타임이 맞는가 |
| 2 | `HitoriEditor` **Release** 빌드 | 성공 | release lib 경로가 맞는가 |
| 3 | 에디터 실행 (`Assets/` 안에 샘플 `.fbx`) | `[FBX] ===== ... =====` → `Axis` → `Texture` → 노드 트리 → `mesh data not implemented yet` | SDK로 파일을 열고 Scene 트리를 읽었는가 |
| 4 | `HitoriGame` 빌드·실행 | 빌드 성공, `no baked data` 경고(B와 동일) | **게임은 FBX SDK를 링크하지 않는다** |
| 5 | 샘플 옆에 `.fbx.bin`이 **없는지** 확인 | 없음 | C단계는 굽기 실패로 처리되어 빈 캐시를 만들지 않는다 |

### 4-3. 출력 예시 (모양만 참고. 이름·숫자는 파일마다 다르다)

```
[FBX] ===== Assets/Character/Male.fbx =====
[FBX] Axis: Up=+Y, RightHanded / Unit: 1 = 1 cm
[FBX] Texture: C:/Users/artist/.../Body_Diffuse.png (relative: Textures/Body_Diffuse.png)
[FBX]   RootNode [-]
[FBX]     Body [Mesh] ControlPoints=8120 Polygons=7954 Skins=1
[FBX]     Armature [Null] Scale=(100, 100, 100)
[FBX]       Hips [Skeleton]
[FBX]         Spine [Skeleton]
[FBX]           ...
[FBX]             Head_End [Skeleton] (no cluster)
[FBX] Assets/Character/Male.fbx: mesh data not implemented yet
```

### 4-4. 막히면

| 증상 | 의심할 곳 |
|---|---|
| `fbxsdk.h`를 열 수 없음 (C1083) | 작업 1-1의 `FbxSdkDir`가 실제 설치 폴더(버전 이름까지)와 같은가 / `SlnGenerator.bat`을 다시 돌렸는가 |
| `libfbxsdk-md.lib`를 열 수 없음 (LNK1104) | lib 폴더 구조가 `lib/x64/debug`인가, `lib/vs2022/x64/debug`인가 → 작업 1-2의 경로를 실제 구조에 맞춘다 |
| 런타임 불일치 (LNK2038 `RuntimeLibrary`) | Debug 구성에 release lib가 걸렸거나 반대. 작업 1-2의 두 `filter` 조건을 확인 |
| `BCryptGenRandom` 확인할 수 없는 외부 기호 (LNK2019, `libxml2-md.lib`) | 작업 1-2의 `links`에 `"bcrypt"`가 있는가 |
| `.pdb`를 찾을 수 없다는 경고 (LNK4099) | SDK가 디버그 심볼을 함께 주지 않아서 나는 경고. 무시해도 된다 |
| `HitoriGame` 빌드에서 FBX 관련 오류 | Runtime 쪽 파일이 `FbxImporter.h`나 `fbxsdk.h`를 include하고 있다. Runtime은 둘 다 몰라야 한다 |
| 에디터 실행 시 `open failed` | 경로에 한글 등 비ASCII 문자가 있는가(SDK는 UTF-8 경로를 기대한다) / 파일이 실제 FBX인가 |
| 트리가 안 찍히고 `no baked data`만 나옴 | 작업 5의 등록이 바뀌지 않았거나, 에디터가 아니라 게임을 실행했다 |

---

## 5. 출력에서 읽어 둘 것 (D단계 준비)

C단계 출력을 보고 아래를 ADR 또는 메모에 적어 둔다. D단계가 이 값들로 결정된다.

| 질문 | 출력 위치 | D단계에서 쓰는 곳 |
|---|---|---|
| 위쪽 축은? 오른손/왼손? | `Axis:` | D-1 축 변환 (엔진: Z-Up, 왼손) |
| 파일 단위는? | `Unit: 1 = ? cm` | A-7: 변환하지 않지만, 크기가 이상하면 원인 판단에 쓴다 |
| 노드에 Scale이 있나? | `Scale=(...)` | A-5: 메시 노드의 전역 행렬을 곱할 때 함께 반영된다 |
| Skin 메시가 몇 개? | `[Mesh] ... Skins=` | A-10: 여러 개면 하나로 합치고 섹션으로 나눈다 |
| Cluster 없는 뼈가 있나? | `(no cluster)` | A-8 대체 값이 실제로 쓰이는가 |
| 텍스처는 외부 파일인가 내장인가? | `Texture: ... (relative: ...)` | A-10 텍스처 경로 규칙 |

---

## 6. 변경 파일 요약

| 파일 | 새/수정 | 작업 |
|---|---|---|
| `premake5.lua` | 수정 — `FbxSdkDir`, `HitoriEditor`의 include·links·libdirs | 1 |
| `Source/Editor/Fbx/FbxImporter.h` | 새 | 2 |
| `Source/Editor/Fbx/FbxImporter.cpp` | 새 | 3·4 |
| `Source/Editor/HitoriEd/EditorEngine.cpp` | 수정 — include 교체, 등록 한 줄 | 5 |
| Runtime 쪽 | **변경 없음** | — |

## 7. 다음 단계와 이어지는 곳

| 위치 | 지금 | 바뀌는 단계 |
|---|---|---|
| `FFbxImporter::Import` 4번 | 트리 출력 후 `false` | **D**: 축 변환·삼각형화·정점 펼치기·머티리얼로 `OutData.Mesh`를 채우고 `true` |
| `PrintNode` / `PrintSceneInfo` | 매 임포트마다 출력 | D 이후 필요하면 Verbose로 내리거나 지운다 |
| `CollectClusterBones` | 이름만 모음 | **E·F·G**: 뼈 배열·가중치·바인드 행렬을 꺼내는 순회의 뼈대가 된다 |

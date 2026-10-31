# FBX Importer B단계 — AssetManager에 FBX 입구 만들기 (따라 하기)

상위 문서: [ADR-0001 FBX Importer 이식](0001-fbx-importer.md)

## 이 문서 읽는 법

- **1~2장**: 왜 이렇게 만드는지(생각의 흐름).
- **3장(작업 0~6)**: 따라 하기. 각 작업은 **파일 → 위치 → 코드** 순서로 적었다.
  - 위치는 "어느 기존 줄 바로 아래"로 적는다. 코드 블록 안의 `// ▼ 추가` ~ `// ▲ 추가` 사이만 새로 쓰고, 그 바깥 줄은 **위치를 찾기 위한 기존 코드**다.
  - **새 파일**은 블록 전체를 그대로 만든다.
- **4장**: 빌드와 확인.
- 빌드는 **작업 6까지 끝난 뒤 한 번** 한다. 중간 작업은 아직 없는 파일을 가리키기 때문이다.

---

## 1. 이 단계가 끝나면

- `Assets` 아래에 `.fbx`를 두고 **에디터**를 켜면 → `[FBX] ...: importer not implemented yet` 경고가 나온다. 아직 진짜 Importer가 없으니 정상이다.
- **게임**을 켜면 → `[FBX] ...: no baked data, import in editor` 경고가 나온다.
- FBX SDK는 아직 쓰지 않는다. B는 **데이터가 지나갈 길만 깐다.**

---

## 2. 생각의 흐름

### 2-1. 출발점: OBJ는 지금 두 층으로 들어온다

```
LoadAsset(".obj")
 └ UAssetManager::LoadObjStaticMesh(Path)       위층: 1. 키 확인  2. 데이터 얻기  3. 만들기·등록
    └ FObjImporter::LoadStaticMeshData(Path)    아래층: 1. 캐시 읽기  2. 굽기  3. 캐시 저장
```

### 2-2. FBX도 같은 두 층

```
LoadAsset(".fbx")
 └ UAssetManager::LoadFbxMesh(Path)                위층: 1. 키 확인  2. 데이터 얻기  3. 만들기·등록(B: 로그)
    └ LoadSkeletalMeshData(Path, Importer)        아래층: 1. 최신성  2. 캐시 읽기  3. 굽는 사람 없음  4. 굽기  5. 캐시 저장
```

OBJ와 다른 점은 셋이고, 모두 **"굽는 일(FBX SDK)은 에디터만 할 수 있다"** 에서 나온다.

| # | 다른 점 | 이유 |
|---|---|---|
| 1 | 아래층이 Importer 안이 아니라 **Runtime(`AssetManager.cpp`)** 에 있고, 굽는 사람을 **인자로** 받는다 | Importer는 에디터에만 있다. 그런데 게임도 캐시는 읽어야 한다 |
| 2 | 아래층 3번 "굽는 사람 없음" | 게임에는 굽는 사람이 없다. 캐시가 없으면 여기서 끝난다 |
| 3 | 아래층 1번 "최신성" | 에디터에서 `.fbx`를 다시 내보내면 자동으로 다시 굽는다. 다시 구울 수 있는 에디터에서만 검사한다 |

### 2-3. 생각 순서 = 작업 순서

```
작업 0. 준비                      새 폴더
작업 1. 무엇을 주고받지?          SkeletalMeshData.h (새)
작업 2. 어디로 들어오지?          AssetManager.cpp — LoadAsset에 .fbx 분기
작업 3. 들어와서 무엇을 하지?     AssetManager.h/.cpp — 위층 LoadFbxMesh, 아래층 LoadSkeletalMeshData
        └ 쓰다 보니 두 가지가 필요하다
작업 4.   캐시를 읽고 써야 한다   BakeIO.h (새), StaticMeshBake.cpp (수정), SkeletalMeshBake.h/.cpp (새)
작업 5.   굽는 사람 자리          AssetManager.h/.cpp — std::function 자리
작업 6. 굽는 사람은 언제 앉지?    Engine.h, LaunchEngineLoop.cpp, EditorEngine.h/.cpp
```

---

## 3. 따라 하기

### 작업 0. 준비

- 폴더를 하나 만든다: `Source/Runtime/Asset/SkeletalMesh/`

---

### 작업 1. 무엇을 주고받지? — 구조체

**파일:** `Source/Runtime/Render/SkeletalMeshData.h` — **새 파일** (`StaticMeshData.h`와 같은 폴더)

```cpp
#pragma once

#include "Render/StaticMeshData.h"
#include "Math/Matrix.h"

// FBX에서 구워 낸 SkeletalMesh 데이터. 에디터가 채우고 런타임이 읽는다.
// 규칙
// 1. SkinWeights[i]는 Mesh.Vertices[i]의 짝 (순서·개수 동일)
// 2. Bones는 부모가 자식보다 앞
// 3. 정점은 모델 공간 (피부를 붙인 순간)
// 4. 스키닝은 행벡터: v' = v * InverseBindPose * BoneTransform

constexpr int32 MaxBoneInfluences = 4;

// 정점 하나가 따라가는 뼈와 비율
struct FSkinWeight
{
	int32 BoneIndex[MaxBoneInfluences] = {};
	float Weight[MaxBoneInfluences] = {};
};

struct FBoneInfo
{
	FString Name;
	int32 ParentIndex = -1;    // 루트는 -1
	FMatrix InverseBindPose;
};

struct FSkeletalMeshData
{
	FStaticMeshData Mesh;              // T-Pose 정점·인덱스·섹션·머티리얼·AABB
	TArray<FSkinWeight> SkinWeights;
	TArray<FBoneInfo> Bones;
};
```

**왜**
- `Mesh`는 기존 `FStaticMeshData`를 그대로 쓴다. T-Pose 메시는 StaticMesh와 같기 때문이다. 그래서 탄젠트, 섹션, 머티리얼, `Validate`, Bake를 재사용할 수 있다.
- 가중치는 `FVertex`(64바이트 고정) 안에 넣지 않고 옆 배열로 둔다. 정점 후처리가 정점의 순서를 바꾸지 않는 것을 확인했으므로 짝이 유지된다.
- B단계에서는 정의만 하고 채우지 않는다.

---

### 작업 2. 어디로 들어오지? — `.fbx` 분기

**파일:** `Source/Runtime/Asset/AssetManager.cpp` — 수정
**위치:** `UAssetManager::LoadAsset` 안, `.obj` 분기 바로 아래

```cpp
void UAssetManager::LoadAsset(const FString& Key, const FString& Path) {
	const FString Extension{ GetAssetExtension(Path) };
	// ... (.png/.dds, .json 분기는 그대로)
	if (Extension == ".obj") {
		LoadObjStaticMesh(Path);
	}
	// ▼ 추가
	if (Extension == ".fbx") {
		LoadFbxMesh(Path);
	}
	// ▲ 추가
}
```

**왜**
- 스캔은 `.fbx.bin` 파일도 만나지만 `.bin` 분기가 없으므로 무시된다. `.obj.bin`과 같다.
- 이름을 `LoadSkeletalMesh`가 아니라 `LoadFbxMesh`로 하는 이유: D단계에서는 FBX를 `UStaticMesh`로 등록한다. 그래서 OBJ의 `LoadObjStaticMesh`처럼 포맷 이름으로 짓는다.

---

### 작업 3. 들어와서 무엇을 하지? — 두 층

#### 3-1. 위층 선언

**파일:** `Source/Runtime/Asset/AssetManager.h` — 수정
**위치:** `public:` 구역의 `static UStaticMesh* LoadObjStaticMesh(...)` 바로 아래

```cpp
	USoundWave* LoadSound(const FString& Path);
	static UStaticMesh* LoadObjStaticMesh(const FString& Path);
	// ▼ 추가
	static UStaticMesh* LoadFbxMesh(const FString& Path);
	// ▲ 추가
```

#### 3-2. include 추가

**파일:** `Source/Runtime/Asset/AssetManager.cpp` — 수정
**위치:** 파일 위쪽, `#include "Asset/ObjImporter/ObjImporter.h"` 바로 아래

```cpp
#include "Asset/ObjImporter/ObjImporter.h"
// ▼ 추가
#include "Asset/SkeletalMesh/SkeletalMeshBake.h"
#include "Render/SkeletalMeshData.h"
// ▲ 추가
```

#### 3-3. 아래층 `LoadSkeletalMeshData`

**파일:** `Source/Runtime/Asset/AssetManager.cpp` — 수정
**위치:** 파일 위쪽의 **익명 네임스페이스** 안, `MakeAssetKey` 함수 바로 아래. 네임스페이스를 닫는 `}`보다 위다.

```cpp
	FString MakeAssetKey(const FString& Path)
	{
		return fs::relative(Path, "Assets").generic_string();
	}

	// ▼ 추가
	// FObjImporter::LoadStaticMeshData와 같은 순서(캐시 읽기 → 굽기 → 캐시 저장).
	// 다른 점: 굽는 사람이 에디터에만 있어서 인자로 받는다.
	TUniquePtr<FSkeletalMeshData> LoadSkeletalMeshData(const FString& Path, const UAssetManager::FSkeletalMeshImporter& Importer)
	{
		const FString BinPath = Path + ".bin";

		// 1. 최신성: .fbx가 .bin보다 새로우면 다시 굽는다 (다시 구울 수 있을 때만)
		bool bStale = false;
		if (Importer)
		{
			std::error_code BinError, FbxError;
			const auto BinTime = fs::last_write_time(BinPath, BinError);
			const auto FbxTime = fs::last_write_time(Path, FbxError);
			bStale = !BinError && !FbxError && FbxTime > BinTime;
		}

		// 2. 캐시 읽기
		if (!bStale)
		{
			if (TUniquePtr<FSkeletalMeshData> Baked = FSkeletalMeshBake::ReadBaked(BinPath))
			{
				return Baked;
			}
		}

		// 3. 굽는 사람 없음 (게임)
		if (!Importer)
		{
			HTR_LOG(Warning, "[FBX] {}: no baked data, import in editor", Path);
			return nullptr;
		}

		// 4. 굽기 (실패 로그는 Importer가 남긴다)
		TUniquePtr<FSkeletalMeshData> Data = MakeUnique<FSkeletalMeshData>();
		if (!Importer(Path, *Data))
		{
			return nullptr;
		}

		// 5. 캐시 저장
		FSkeletalMeshBake::WriteBaked(BinPath, *Data);
		return Data;
	}
	// ▲ 추가
}
```

**왜**
- **1. 최신성**: `last_write_time`은 `error_code` 버전을 쓴다. `.bin`이 없으면 예외 대신 `BinError`가 켜지고 `bStale`은 `false`가 된다. 그러면 2번 `ReadBaked`가 "파일 없음"으로 `nullptr`를 돌려주고 4번 굽기로 넘어간다.
- **3. 굽는 사람 없음**: OBJ에는 없는 단계다. OBJ는 파서가 항상 있지만, FBX는 게임에 굽는 사람이 없다.
- **4. 굽기**: 실패 로그를 굽는 쪽이 남기는 것은 OBJ의 `Cook`이 `HTR_LOG(Error, ...)`를 직접 남기는 것과 같다.
- 최신성 검사가 막지 **못하는** 경우: Importer **코드**를 고쳐서 출력이 바뀌었는데 `.fbx`는 그대로일 때다. 이때는 `BakeVersion`을 올린다(작업 4-4).

#### 3-4. 위층 `LoadFbxMesh`

**파일:** `Source/Runtime/Asset/AssetManager.cpp` — 수정
**위치:** `UAssetManager::LoadObjStaticMesh` 함수가 끝나는 `}` 바로 아래, `UAssetManager::ReorientStaticMesh`보다 위

```cpp
	Get().RegisterAsset(Key, Mesh);
	return Mesh;
}

// ▼ 추가
UStaticMesh* UAssetManager::LoadFbxMesh(const FString& Path)
{
	// 1. 키 확인
	const FString Key = MakeAssetKey(Path);
	if (UStaticMesh* Cached = GetAssetByPath<UStaticMesh>(Key))
	{
		return Cached;
	}

	// 2. 데이터 얻기
	TUniquePtr<FSkeletalMeshData> Data = LoadSkeletalMeshData(Path, SkeletalMeshImporter);
	if (!Data)
	{
		return nullptr;
	}

	// 3. 만들기·등록
	// shortcut: B단계는 로그만 남긴다. D단계에서 CreateStaticMesh + RegisterAsset, H단계에서 USkeletalMesh 분기(반환형도 그때 넓힌다).
	HTR_LOG(Info, "[FBX] {}: loaded (vertices {}, bones {})", Path, Data->Mesh.Vertices.Num(), Data->Bones.Num());
	return nullptr;
}
// ▲ 추가

bool UAssetManager::ReorientStaticMesh(UStaticMesh& Mesh, const std::function<FVector(const FVector&)>& Rotate)
```

**왜**
- `LoadObjStaticMesh`와 나란히 놓으면 **2번의 호출 대상만** 다르다.

| 단계 | `LoadObjStaticMesh` | `LoadFbxMesh` |
|---|---|---|
| 1. 키 확인 | `GetAssetByPath<UStaticMesh>(Key)` | 같음 |
| 2. 데이터 얻기 | `FObjImporter::LoadStaticMeshData(Path)` | `LoadSkeletalMeshData(Path, SkeletalMeshImporter)` |
| 3. 만들기·등록 | `CreateStaticMesh` → `RegisterAsset` | B: 로그 / D: 같음 / H: 분기 |

**여기까지 쓰면** 아직 없는 것 두 가지가 보인다: `FSkeletalMeshBake`(작업 4), `SkeletalMeshImporter`(작업 5).

---

### 작업 4. 캐시를 읽고 써야 한다 — Bake

#### 4-1. 읽기·쓰기 도구를 공용 헤더로

**파일:** `Source/Runtime/Asset/BakeIO.h` — **새 파일**

내용은 `StaticMeshBake.cpp` 익명 네임스페이스에 있던 도구 6개를 **그대로 옮긴 것**이다. 바뀐 점은 `namespace BakeIO`로 감싼 것과, 템플릿이 아닌 두 함수에 `inline`을 붙인 것뿐이다.

```cpp
#pragma once

#include <fstream>
#include <type_traits>

#include "Core/Types.h"
#include "Core/EngineString.h"
#include "Containers/Array.h"

// .bin 캐시 읽기·쓰기 도구. StaticMeshBake와 SkeletalMeshBake가 같이 쓴다.
namespace BakeIO
{
	// 숫자 || 숫자만 있는 구조체
	template<typename T>
	void WritePod(std::ofstream& Out, const T& Value)
	{
		static_assert(std::is_trivially_copyable_v<T>, "invalid plane old data");
		Out.write(reinterpret_cast<const char*>(&Value), sizeof(T));
	}

	template<typename T>
	bool ReadPod(std::ifstream& In, T& Value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		In.read(reinterpret_cast<char*>(&Value), sizeof(T));
		return static_cast<bool>(In);
	}

	// 배열: 개수 + 원소 전체를 한 번에
	template<typename T>
	void WriteArray(std::ofstream& Out, const TArray<T>& Array)
	{
		const uint32 Count = static_cast<uint32>(Array.Num());
		WritePod(Out, Count);
		if (Count > 0)
		{
			Out.write(reinterpret_cast<const char*>(Array.GetData()), sizeof(T) * Count);
		}
	}

	template<typename T>
	bool ReadArray(std::ifstream& In, TArray<T>& Array, uint32 MaxCount)
	{
		uint32 Count = 0;
		if (!ReadPod(In, Count) || Count > MaxCount)
		{
			return false;
		}

		Array.SetNum(static_cast<int32>(Count));
		if (Count > 0)
		{
			In.read(reinterpret_cast<char*>(Array.GetData()), sizeof(T) * Count);
		}
		return static_cast<bool>(In);
	}

	// 길이 + 문자열
	inline void WriteString(std::ofstream& Out, const FString& Str)
	{
		const uint32 Length = static_cast<uint32>(Str.size());
		WritePod(Out, Length);
		Out.write(Str.data(), Length);
	}

	inline bool ReadString(std::ifstream& In, FString& Str)
	{
		uint32 Length = 0;
		if (!ReadPod(In, Length) || Length > 4096)
		{
			return false;
		}
		Str.resize(Length);
		In.read(Str.data(), Length);
		return static_cast<bool>(In);
	}
}
```

**왜**
- 복사하지 않고 **옮긴다.** 같은 코드가 두 곳에 있으면 한쪽만 고치는 실수가 생기기 때문이다.
- `inline`을 붙이는 이유: 이 헤더를 여러 .cpp가 include해도 중복 정의 링크 오류가 나지 않게 하기 위해서다. 템플릿은 원래 괜찮다.

#### 4-2. `StaticMeshBake.cpp`에서 도구를 지우고 헤더를 쓰게 하기

**파일:** `Source/Runtime/Asset/ObjImporter/StaticMeshBake.cpp` — 수정

할 일은 세 가지다.
1. `#include "Core/EngineLog.h"` 바로 아래에 `#include "Asset/BakeIO.h"`를 추가한다.
2. `static_assert` 묶음 바로 아래(익명 `namespace {` 위)에 `using namespace BakeIO;`를 추가한다.
3. 익명 네임스페이스 안에서 `// 숫자 || 숫자만 있는 구조체`부터 `ReadString` 함수 끝까지 **6개 함수를 지운다.** `BakeVersion`과 `FBakeHeader`는 남긴다.

수정 후 파일 위쪽은 이렇게 된다(`FStaticMeshBake::ReadBaked`부터 아래는 **한 글자도 바꾸지 않는다**).

```cpp
#include "EnginePCH.h"
#include "StaticMeshBake.h"

#include <cstring>
#include <fstream>
#include <type_traits>

#include "Render/StaticMeshData.h"
#include "Core/EngineLog.h"
#include "Asset/BakeIO.h"                 // 추가

static_assert(sizeof(FVertex) == 64);

// 바이트 그대로 읽고 쓰는 타입. 복사 생성자를 직접 작성하면 여기서 컴파일 오류가 난다
static_assert(std::is_trivially_copyable_v<FVertex>);
static_assert(std::is_trivially_copyable_v<FStaticMeshSection>);
static_assert(std::is_trivially_copyable_v<FVector4>);
static_assert(std::is_trivially_copyable_v<FBox>);

static_assert(std::is_trivially_copyable_v<FLODSourceVertex>);

using namespace BakeIO;                   // 추가

namespace
{
	// FStaticMeshData, FVertex, FStaticMeshSection, FStaticMaterialSlot 구조가 바뀌면 올린다
	constexpr uint32 BakeVersion{ 5 };

	struct FBakeHeader
	{
		// 'S'taic 'M'esh 'B'a'K'e
		char Magic[4] = { 'S', 'M', 'B', 'K' };
		uint32 Version = BakeVersion;
	};

	// (WritePod ~ ReadString 6개 함수는 지웠다 → Asset/BakeIO.h)
}

TUniquePtr<FStaticMeshData> FStaticMeshBake::ReadBaked(const FString& BinPath)
// ... 이하 그대로
```

**왜**
- `ReadBaked`/`WriteBaked`는 `WritePod<FBakeHeader>(...)`처럼 함수 이름을 그대로 부른다. `using namespace BakeIO;` 한 줄로 그 호출들이 수정 없이 동작한다.
- 동작과 바이트 형식은 그대로다. 그래서 `BakeVersion{ 5 }`도 올리지 않는다.

#### 4-3. SkeletalMesh Bake 선언

**파일:** `Source/Runtime/Asset/SkeletalMesh/SkeletalMeshBake.h` — **새 파일** (`StaticMeshBake.h`와 같은 모양)

```cpp
#pragma once

#include "Core/Types.h"
#include "Core/EngineString.h"

struct FSkeletalMeshData;

// FSkeletalMeshData를 .fbx.bin으로 저장하고 읽는다.
class FSkeletalMeshBake
{
public:
	static TUniquePtr<FSkeletalMeshData> ReadBaked(const FString& BinPath);
	static void WriteBaked(const FString& BinPath, const FSkeletalMeshData& Data);
};
```

#### 4-4. SkeletalMesh Bake 구현 (헤더만)

**파일:** `Source/Runtime/Asset/SkeletalMesh/SkeletalMeshBake.cpp` — **새 파일**

```cpp
#include "EnginePCH.h"
#include "SkeletalMeshBake.h"

#include <cstring>

#include "Asset/BakeIO.h"
#include "Render/SkeletalMeshData.h"
#include "Core/EngineLog.h"

using namespace BakeIO;

// 바이트 그대로 읽고 쓰는 타입
static_assert(std::is_trivially_copyable_v<FSkinWeight>);

namespace
{
	// FSkeletalMeshData 구조나 Importer 출력이 바뀌면 올린다
	constexpr uint32 BakeVersion{ 1 };

	struct FBakeHeader
	{
		// 'S'keletal 'K' 'B'a'K'e
		char Magic[4] = { 'S', 'K', 'B', 'K' };
		uint32 Version = BakeVersion;
	};
}

TUniquePtr<FSkeletalMeshData> FSkeletalMeshBake::ReadBaked(const FString& BinPath)
{
	std::ifstream In(BinPath, std::ios::binary);
	if (!In)
	{
		return nullptr;   // .bin이 없음 (처음 로드)
	}

	// 1. 우리 형식·같은 버전인지 확인
	FBakeHeader Header;
	const FBakeHeader Expected;
	if (!ReadPod(In, Header) ||
		std::memcmp(Header.Magic, Expected.Magic, sizeof(Header.Magic)) != 0 ||
		Header.Version != Expected.Version)
	{
		HTR_LOG(Warning, "[BAKE] {}: invalid header or version, re-importing", BinPath);
		return nullptr;
	}

	// shortcut: 본문(메시·뼈·가중치) 읽기는 D~F단계에서 추가한다. 그 전까지는 항상 다시 굽는다.
	return nullptr;
}

void FSkeletalMeshBake::WriteBaked(const FString& BinPath, const FSkeletalMeshData& Data)
{
	std::ofstream Out(BinPath, std::ios::binary);
	if (!Out)
	{
		HTR_LOG(Warning, "[BAKE] {}: cannot open for writing", BinPath);
		return;
	}

	// 1. 헤더
	WritePod(Out, FBakeHeader());

	// shortcut: 본문 쓰기는 D~F단계에서 추가한다.

	// 스트림은 한 번 실패하면 상태가 유지되므로 마지막에 한 번만 확인
	if (!Out)
	{
		HTR_LOG(Warning, "[BAKE] {}: write failed", BinPath);
	}
}
```

**왜**
- 첫 줄은 반드시 `#include "EnginePCH.h"`여야 한다. `HitoriEngine` 프로젝트의 PCH 설정 때문이다.
- magic을 `SMBK`(Static)와 다르게 `SKBK`로 둔 이유: 다른 종류의 `.bin`을 실수로 읽어도 헤더에서 바로 걸러진다.
- B단계에서는 굽는 사람이 항상 실패하므로 `WriteBaked`는 아직 호출되지 않는다.

---

### 작업 5. 굽는 사람 자리 — `std::function`

#### 5-1. 전방 선언

**파일:** `Source/Runtime/Asset/AssetManager.h` — 수정
**위치:** `class USoundWave;` 바로 아래

```cpp
class UStaticMesh;
class USoundWave;
// ▼ 추가
struct FSkeletalMeshData;
// ▲ 추가
```

#### 5-2. 자리 선언 (public)

**파일:** `Source/Runtime/Asset/AssetManager.h` — 수정
**위치:** 작업 3-1에서 추가한 `LoadFbxMesh` 선언 바로 아래

```cpp
	static UStaticMesh* LoadObjStaticMesh(const FString& Path);
	static UStaticMesh* LoadFbxMesh(const FString& Path);
	// ▼ 추가
	// FBX 경로를 받아 FSkeletalMeshData를 채운다. 에디터만 등록하고, 게임에서는 비어 있다.
	using FSkeletalMeshImporter = std::function<bool(const FString& FbxPath, FSkeletalMeshData& OutData)>;
	static void SetSkeletalMeshImporter(FSkeletalMeshImporter InImporter);
	// ▲ 추가
```

#### 5-3. 자리 변수 (private)

**파일:** `Source/Runtime/Asset/AssetManager.h` — 수정
**위치:** 아래쪽 `private:` 구역의 `TMap<FString, USoundWave*> SoundMap;` 바로 아래

```cpp
private:
	TMap<FString, FString> AssetPathMap;
	TMap<FString, URenderAsset*> AssetMap;
	TMap<FString, USoundWave*> SoundMap;
	// ▼ 추가
	static FSkeletalMeshImporter SkeletalMeshImporter;
	// ▲ 추가
```

#### 5-4. 자리 정의 + setter

**파일:** `Source/Runtime/Asset/AssetManager.cpp` — 수정
**위치:** 익명 네임스페이스를 닫는 `}` 바로 아래, `UAssetManager& UAssetManager::Get()` 위

```cpp
	// ... (작업 3-3의 LoadSkeletalMeshData 끝)
}

// ▼ 추가
UAssetManager::FSkeletalMeshImporter UAssetManager::SkeletalMeshImporter;

void UAssetManager::SetSkeletalMeshImporter(FSkeletalMeshImporter InImporter)
{
	SkeletalMeshImporter = std::move(InImporter);
}
// ▲ 추가

UAssetManager& UAssetManager::Get()
```

**왜**
- `std::function`은 이 클래스가 이미 쓰는 방식이다(`FAssetLoadProgress`).
- **static**으로 두는 이유: 등록이 `UAssetManager::Init`보다 먼저 일어난다(작업 6). 인스턴스(`Get()`)를 거치지 않아야 순서를 신경 쓸 필요가 없다.
- `FSkeletalMeshData`는 헤더에서 전방 선언만 한다. 참조로만 쓰이므로 충분하다.
- 아래층에는 이 변수를 **인자로** 넘긴다. 그래서 아래층은 익명 네임스페이스의 일반 함수로 둘 수 있다.

---

### 작업 6. 굽는 사람은 언제 앉지? — 스캔 전

`LoadFbxMesh`는 스캔 도중 호출된다. 그러니 굽는 사람은 그보다 먼저 자리에 있어야 한다.

```
FEngineLoop::PreInit
 1. GEngine = NewObject<UEngine>(EngineClass)   ← 에디터 엔진 객체가 이미 있다
 2. 렌더 장치·창 생성
 3. GEngine->RegisterAssetImporters()            ← 추가 (굽는 사람 등록)
 4. UAssetManager::Init → ScanAssets             ← .fbx를 만나 LoadFbxMesh
 5. GEngine->Init()                              ← 여기는 이미 늦다
```

#### 6-1. 시점 만들기

**파일:** `Source/Runtime/Engine/Engine.h` — 수정
**위치:** `class UEngine`의 `virtual FEngineConfig GetConfig() const { return {}; }` 바로 아래

```cpp
public:
	// 창을 만들기 전에 호출된다.
	virtual FEngineConfig GetConfig() const { return {}; }

	// ▼ 추가
	// AssetManager가 에셋을 스캔하기 전에 호출된다. 에셋 임포터를 등록하는 자리다.
	virtual void RegisterAssetImporters() {}
	// ▲ 추가

	// Device·Window·Swapchain·AssetManager가 준비된 뒤 호출된다.
	virtual bool Init();
```

- 기본 구현은 **비어 있다.** 게임 엔진은 override하지 않으므로 굽는 사람이 없다.

#### 6-2. 시점에 호출

**파일:** `Source/Runtime/Launch/LaunchEngineLoop.cpp` — 수정
**위치:** `FEngineLoop::PreInit` 안, `FRenderResourceManager::Init();` 바로 아래, `constexpr float AssetStart` 위

```cpp
	FSplashScreen::SetText(L"Loading Assets...");
	FRenderResourceManager::Init();

	// ▼ 추가
	// 스캔 전에 임포터 등록 (에디터만 실제로 등록한다)
	GEngine->RegisterAssetImporters();
	// ▲ 추가

	constexpr float AssetStart = 0.1f;
	constexpr float AssetEnd = 0.8f;
	UAssetManager::Init([](int32 Loaded, int32 Total, const FString& Path)
```

#### 6-3. 에디터 override 선언

**파일:** `Source/Editor/HitoriEd/EditorEngine.h` — 수정
**위치:** `class UEditorEngine`의 `FEngineConfig GetConfig() const override;` 바로 아래

```cpp
public:
	FEngineConfig GetConfig() const override;
	// ▼ 추가
	void RegisterAssetImporters() override;
	// ▲ 추가
	bool Init() override;
```

#### 6-4. 에디터 override 구현 (가짜 굽는 사람)

**파일:** `Source/Editor/HitoriEd/EditorEngine.cpp` — 수정

(1) **include 위치:** `#include "Asset/AssetManager.h"` 바로 아래

```cpp
#include "Asset/AssetManager.h"
// ▼ 추가
#include "Render/SkeletalMeshData.h"
// ▲ 추가
#include "Render/RenderResourceManager.h"
```

(2) **함수 위치:** `UEditorEngine::GetConfig()` 정의가 끝나는 `}` 바로 아래, `// 렌더 자원·월드·에디터와 MultipleViewports 연결을 초기화한다.` 주석 위

```cpp
	Desc.SplashImage = "Resources/Splash.png";
	return Desc;
}

// ▼ 추가
// 1. FBX 임포터 등록
// shortcut: B단계는 가짜(로그 후 실패). C단계에서 FFbxImporter::Import로 바꾼다.
void UEditorEngine::RegisterAssetImporters()
{
	UAssetManager::SetSkeletalMeshImporter([](const FString& FbxPath, FSkeletalMeshData&)
	{
		HTR_LOG(Warning, "[FBX] {}: importer not implemented yet", FbxPath);
		return false;
	});
}
// ▲ 추가

// 렌더 자원·월드·에디터와 MultipleViewports 연결을 초기화한다.
// Device·Window·Swapchain·AssetManager는 FEngineLoop가 먼저 만들어 둔다.
bool UEditorEngine::Init()
```

**왜**
- 의존 방향이 **Editor → Runtime** 한 방향뿐이다. 런타임(`AssetManager`, `Engine`, `LaunchEngineLoop`)은 에디터도 FBX도 모른다.
- 실패 로그를 굽는 쪽(이 람다)이 남긴다. 아래층의 "실패 로그는 Importer가 남긴다"는 약속과 같다.
- UE에서는 런타임의 `USkeletalMesh::BuildLODModel`이 `#if WITH_EDITOR` 안에서 `IMeshBuilderModule`을 부른다. 우리 엔진에는 모듈 매니저와 `WITH_EDITOR` 구분이 없으므로 **"런타임이 빈칸을 선언하고 에디터가 채운다"** 로 같은 효과를 낸다.

---

## 4. 빌드와 확인

### 4-1. 프로젝트 파일 다시 만들기

새 파일이 4개(`SkeletalMeshData.h`, `BakeIO.h`, `SkeletalMeshBake.h`, `SkeletalMeshBake.cpp`) 생겼다.
1. Visual Studio를 닫는다.
2. 저장소 루트의 **`SlnGenerator.bat`을 실행**한다. premake가 `Source/Runtime/**`를 다시 모아 `.vcxproj`에 새 파일을 넣는다.
3. `Hitori.slnx`를 다시 열고 Solution Explorer에서 새 파일 4개가 보이는지 확인한다.

### 4-2. 빌드

- `HitoriEditor`(Debug)를 빌드한다.
- `HitoriGame`을 빌드한다.

### 4-3. 확인

| # | 할 일 | 기대 결과 | 무엇을 확인하나 |
|---|---|---|---|
| 1 | `Assets/` 아래에 샘플 `.fbx`를 두고 **에디터** 실행 | Output Log에 `[FBX] Assets/...: importer not implemented yet` | 스캔 → 위층 → 아래층 → 굽는 사람 호출까지 길이 이어졌는가 |
| 2 | **HitoriGame** 실행 | `[FBX] ...: no baked data, import in editor`, 크래시 없음 | 게임에는 굽는 사람이 없는가 (아래층 3번) |
| 3 | 기존 OBJ가 있는 상태로 실행 | `[BAKE] ...obj.bin: loaded` (`re-cooking`이 아님) | 작업 4-2의 도구 이동이 바이트 형식을 바꾸지 않았는가 |
| 4 | `HitoriGame` 빌드 결과 | 성공 | B단계에 SDK 의존이 없는가 |

### 4-4. 막히면

| 증상 | 의심할 곳 |
|---|---|
| `FSkeletalMeshImporter`를 찾을 수 없음 | 작업 5-2의 `using`이 **`public:`** 구역에 있는가 (아래층이 `UAssetManager::FSkeletalMeshImporter`로 접근한다) |
| `SkeletalMeshImporter` 미해결 외부 기호(LNK2001) | 작업 5-4의 정의 줄 `UAssetManager::FSkeletalMeshImporter UAssetManager::SkeletalMeshImporter;`을 빠뜨렸는가 |
| `WritePod` 등 중복 정의(LNK2005) | `BakeIO.h`의 `WriteString`·`ReadString`에 `inline`이 있는가 / `StaticMeshBake.cpp`에서 6개 함수를 지웠는가 |
| 헤더 툴(`HitoriHeaderTool`) 오류 | `AssetManager.h`의 `UCLASS` 안에 새로 넣은 `using`이나 `static` 멤버 줄. 오류가 나면 `static` 변수를 `AssetManager.cpp`의 파일 범위 변수로 옮긴다 |
| 에디터에서 1번 로그가 안 나옴 | 작업 6-2의 호출이 `UAssetManager::Init`보다 **위**에 있는가 / 샘플이 `Assets/` 안에 있는가 |
| 새 파일이 Solution Explorer에 없음 | 4-1의 `SlnGenerator.bat`을 실행했는가 |

---

## 5. 변경 파일 요약

| 파일 | 새/수정 | 작업 |
|---|---|---|
| `Source/Runtime/Render/SkeletalMeshData.h` | 새 | 1 |
| `Source/Runtime/Asset/AssetManager.cpp` | 수정 — `.fbx` 분기, include 2줄, 아래층(익명 네임스페이스), 자리 정의·setter, 위층 | 2, 3-2, 3-3, 5-4, 3-4 |
| `Source/Runtime/Asset/AssetManager.h` | 수정 — 전방 선언, `LoadFbxMesh`, `using`·setter, static 변수 | 3-1, 5-1, 5-2, 5-3 |
| `Source/Runtime/Asset/BakeIO.h` | 새 | 4-1 |
| `Source/Runtime/Asset/ObjImporter/StaticMeshBake.cpp` | 수정 — include·using 추가, 함수 6개 삭제 | 4-2 |
| `Source/Runtime/Asset/SkeletalMesh/SkeletalMeshBake.h` | 새 | 4-3 |
| `Source/Runtime/Asset/SkeletalMesh/SkeletalMeshBake.cpp` | 새 | 4-4 |
| `Source/Runtime/Engine/Engine.h` | 수정 — virtual 1줄 | 6-1 |
| `Source/Runtime/Launch/LaunchEngineLoop.cpp` | 수정 — 호출 1줄 | 6-2 |
| `Source/Editor/HitoriEd/EditorEngine.h` | 수정 — override 선언 | 6-3 |
| `Source/Editor/HitoriEd/EditorEngine.cpp` | 수정 — include, override 구현 | 6-4 |

---

## 6. 다음 단계와 이어지는 곳

B단계에서 `shortcut:` 주석을 단 세 곳이 이후 단계의 입구다.

| 위치 | 지금 | 바뀌는 단계 |
|---|---|---|
| `UEditorEngine::RegisterAssetImporters`의 람다 | 로그 후 `false` | **C**: `FFbxImporter::Import`로 교체 |
| `FSkeletalMeshBake::ReadBaked/WriteBaked` | 헤더만 | **D**: 메시 본문, **E**: 뼈, **F**: 가중치 (구조가 바뀔 때마다 `BakeVersion` 증가) |
| `LoadFbxMesh` 3번 | 로그만 | **D**: `CreateStaticMesh` + `RegisterAsset`, **H**: 뼈 유무로 Static/Skeletal 분기 (반환형을 `URenderAsset*`로 넓힘) |

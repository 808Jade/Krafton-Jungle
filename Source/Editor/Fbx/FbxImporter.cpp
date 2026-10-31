#include "EnginePCH.h"
#include "Editor/Fbx/FbxImporter.h"

#include <fbxsdk.h>
#include <algorithm>
#include <filesystem>
#include <format>
#include <map>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Render/SkeletalMeshData.h"
#include "Core/EngineLog.h"
/*
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
*/
namespace
{
	struct FFbxManagerDeleter
	{
		void operator()(FbxManager* Manager) const { Manager->Destroy(); }
	};
	using FFbxManagerPtr = std::unique_ptr<FbxManager, FFbxManagerDeleter>;
	

	const char* GetAttributeName(FbxNode* Node)
	{
		//Fbx파일에 있는 값 추출
		const FbxNodeAttribute* Attribute = Node->GetNodeAttribute();
		if (!Attribute)
		{
			return "-";
		}
	
		switch (Attribute->GetAttributeType())
		{
			case FbxNodeAttribute::eMesh:	  return "Mesh";
			case FbxNodeAttribute::eSkeleton: return "Skeleton";
			case FbxNodeAttribute::eNull:     return "Null";
			case FbxNodeAttribute::eCamera:   return "Camera";
			case FbxNodeAttribute::eLight:    return "Light";
			default:                          return "Other";
		}
	}

	//Mesh → Skin → Cluster → Link(뼈 노드)
	void CollectClusterBones(FbxNode* Node, std::unordered_set<FString>& OutBones)
	{
		if (FbxMesh* Mesh = Node->GetMesh())
		{
			const int32 SkinCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
			for (int32 SkinIndex = 0;SkinIndex < SkinCount;++SkinIndex)
			{
				FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(SkinIndex, FbxDeformer::eSkin));
				for (int32 ClusterIndex = 0;ClusterIndex < Skin->GetClusterCount();++ClusterIndex)
				{
					if (FbxNode* Bone = Skin->GetCluster(ClusterIndex)->GetLink())
					{
						OutBones.insert(Bone->GetName());
					}
				}
			}
		}
		for (int32 ChildIndex = 0;ChildIndex < Node->GetChildCount();++ChildIndex)
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

		// 2. 메시 크기 정보
		if (FbxMesh* Mesh = Node->GetMesh())
		{
			Line += std::format(" ControlPoints={} Polygons={} Skins={}",
				Mesh->GetControlPointsCount(), Mesh->GetPolygonCount(), Mesh->GetDeformerCount(FbxDeformer::eSkin));
		}

		// 3. Cluster 없는 뼈 (A-8: 바인드 행렬을 0프레임 전역 행렬로 대신)
		if (Node->GetSkeleton() && !ClusterBones.contains(Node->GetName()))
		{
			Line += " (no cluster)";
		}

		// 4. 노드 Scale (1이 아닐 때만)
		const FbxDouble3 Scale = Node->LclScaling.Get();
		if (Scale[0] != 1.0 || Scale[1] != 1.0 || Scale[2] != 1.0)
		{
			Line += std::format(" Scale=({}, {}, {})", Scale[0], Scale[1], Scale[2]);
		}

		HTR_LOG(Info, "[FBX]   {}", Line);

		// 5. 자식으로 재귀
		for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
		{
			PrintNode(Node->GetChild(ChildIndex), Depth + 1, ClusterBones);
		}
	}

	void PrintSceneInfo(FbxScene* Scene)
	{
		// 1. 파일 전역 설정
		FbxGlobalSettings& Settings = Scene->GetGlobalSettings();   // SDK 객체: 복사 금지 → 참조로
		FbxAxisSystem Axis = Settings.GetAxisSystem();              // 값 타입: 복사본으로
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

	FMatrix MakeAxisConversion(FbxScene* Scene, const FString& FbxPath)
	{
		// 1. 파일의 위쪽 축과 손 방향
		FbxAxisSystem Axis = Scene->GetGlobalSettings().GetAxisSystem();   // 값 타입: 복사본으로 (non-const 함수라 const 금지)
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
				1.0f, 0.0f, 0.0f, 0.0f,     // 파일 X → 엔진 X
				0.0f, -1.0f, 0.0f, 0.0f,     // 파일 Y → 엔진 -Y
				0.0f, 0.0f, 1.0f, 0.0f,     // 파일 Z → 엔진 Z
				0.0f, 0.0f, 0.0f, 1.0f);
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
	struct FVertexSource
	{
		FbxMesh* Mesh = nullptr;
		int32 ControlPoint = -1;
	};
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
		FbxGeometryElementMaterial* MaterialElement = Mesh->GetElementMaterial();   // GetIndexArray()가 non-const라 const 금지
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

	// ===================== E. 뼈 =====================

	// 뼈 수집 (전위 순회: 부모를 먼저 넣으므로 "부모가 앞" 규칙이 저절로 지켜진다)
	// - 뼈 판정: Skeleton 꼬리표, 또는 Cluster가 가리키는 노드 (Null 노드를 뼈로 쓰는 파일 대비)
	void CollectBones(FbxNode* Node, int32 ParentBone, const std::unordered_set<FString>& ClusterBones,
		TArray<FBoneInfo>& OutBones, std::unordered_map<FbxNode*, int32>& OutBoneIndexByNode)
	{
		int32 ThisBone = ParentBone;

		// [if] 뼈 노드면 배열에 추가. 자식들은 ThisBone을 부모로 받는다
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

	// ===================== F. 가중치 =====================

	// 메시의 모든 Cluster 방문 (Mesh → Skin → Cluster). F·G가 같이 쓴다
	template <typename FFunc>
	void ForEachCluster(FbxMesh* Mesh, FFunc&& Func)
	{
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

	// 영향 목록 → FSkinWeight (큰 순, 최대 MAX_TOTAL_INFLUENCES개, 합 = 1)
	FSkinWeight MakeSkinWeight(FInfluences Influences)
	{
		FSkinWeight Out;

		// 1. 큰 순 정렬
		std::sort(Influences.begin(), Influences.end(),
			[](const auto& A, const auto& B) { return A.second > B.second; });

		// 2. 최대 개수로 자르기
		if (Influences.size() > MAX_TOTAL_INFLUENCES)
		{
			Influences.resize(MAX_TOTAL_INFLUENCES);
		}

		// 3. 합
		float Sum = 0.0f;
		for (const auto& Influence : Influences)
		{
			Sum += Influence.second;
		}

		// [if] 영향이 없는 정점 → 0번 뼈(루트)에 고정 (합이 0이면 스키닝 결과가 원점으로 빨려 들어간다)
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
		for (FbxNode* Node : MeshNodes)
		{
			FbxMesh* Mesh = Node->GetMesh();
			std::vector<FInfluences>& PerControlPoint = InfluencesByMesh[Mesh];
			PerControlPoint.resize(Mesh->GetControlPointsCount());

			// [for] Cluster마다 (= 뼈마다): 이 뼈가 움직이는 ControlPoint와 비율을 ControlPoint 쪽으로 옮겨 담기
			ForEachCluster(Mesh, [&](FbxCluster* Cluster)
			{
				const auto Bone = BoneIndexByNode.find(Cluster->GetLink());
				if (Bone == BoneIndexByNode.end())
				{
					return;
				}

				const int32 Count = Cluster->GetControlPointIndicesCount();
				const int* ControlPoints = Cluster->GetControlPointIndices();
				const double* Weights = Cluster->GetControlPointWeights();
				for (int32 i = 0; i < Count; ++i)
				{
					if (Weights[i] > 0.0 && ControlPoints[i] >= 0 && ControlPoints[i] < static_cast<int32>(PerControlPoint.size()))
					{
						PerControlPoint[ControlPoints[i]].push_back({ Bone->second, static_cast<float>(Weights[i]) });
					}
				}
			});
		}

		// 2. 펼친 정점마다 자기 ControlPoint의 목록 복사 (Sources = D에서 남긴 다리, 순서 = 정점 순서)
		int32 Unweighted = 0;
		for (const FVertexSource& Source : Sources)
		{
			const FInfluences& Influences = InfluencesByMesh[Source.Mesh][Source.ControlPoint];
			if (Influences.empty())
			{
				++Unweighted;
			}
			OutWeights.Add(MakeSkinWeight(Influences));
		}

		if (Unweighted > 0)
		{
			HTR_LOG(Warning, "[FBX] {}: {} vertices have no weights, bound to root", FbxPath, Unweighted);
		}
	}

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

		// 2. Cluster 기록(피부를 붙인 순간 뼈의 전역 행렬)으로 덮어쓰기
		for (FbxNode* Node : MeshNodes)
		{
			ForEachCluster(Node->GetMesh(), [&](FbxCluster* Cluster)
			{
				const auto Bone = BoneIndexByNode.find(Cluster->GetLink());
				if (Bone != BoneIndexByNode.end())
				{
					Cluster->GetTransformLinkMatrix(BindGlobal[Bone->second]);
				}
			});
		}

		// 3. 엔진 공간 → 역행렬
		for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
		{
			const FMatrix Bind = ConvertMatrix(BindGlobal[BoneIndex], Axis);
			Bones[BoneIndex].InverseBindPose = Bind.Inverse();
		}
	}

	// 검사: 뼈 위치가 메시 범위(+여유 25%) 안에 있는가 (정점과 뼈가 같은 공간·같은 축인지)
	int32 CountBonesOutsideMesh(const FSkeletalMeshData& Data)
	{
		const FBox& Box = Data.Mesh.AABB;
		const FVector Margin = (Box.Max - Box.Min) * 0.25f;
		int32 Outside = 0;
		for (const FBoneInfo& Bone : Data.Bones)
		{
			FMatrix Bind = Bone.InverseBindPose.Inverse();
			const FVector4 Origin = Bind.GetOrigin();
			if (Origin.X < Box.Min.X - Margin.X || Origin.X > Box.Max.X + Margin.X ||
				Origin.Y < Box.Min.Y - Margin.Y || Origin.Y > Box.Max.Y + Margin.Y ||
				Origin.Z < Box.Min.Z - Margin.Z || Origin.Z > Box.Max.Z + Margin.Z)
			{
				++Outside;
			}
		}
		return Outside;
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

	// 6. Cluster 뼈 수집 → 노드 트리 출력 (C)
	std::unordered_set<FString> ClusterBones;
	CollectClusterBones(Scene->GetRootNode(), ClusterBones);
	PrintNode(Scene->GetRootNode(), 0, ClusterBones);

	// 7. 삼각형화 (D, SDK): Skin은 새 메시로 옮겨진다
	FbxGeometryConverter Converter(Manager.get());
	if (!Converter.Triangulate(Scene, true))
	{
		HTR_LOG(Warning, "[FBX] {}: triangulate failed on some meshes", FbxPath);
	}

	// 8. 축 변환 행렬 (D)
	const FMatrix Axis = MakeAxisConversion(Scene, FbxPath);

	// 9. 뼈 배열 (E)
	std::unordered_map<FbxNode*, int32> BoneIndexByNode;
	CollectBones(Scene->GetRootNode(), -1, ClusterBones, OutData.Bones, BoneIndexByNode);
	PrintBones(OutData.Bones);

	// 10. 메시: 정점 펼치기·머티리얼·섹션 (D)
	std::vector<FbxNode*> MeshNodes;
	CollectMeshNodes(Scene->GetRootNode(), MeshNodes);

	const bool bAnySkin = !ClusterBones.empty();
	std::vector<std::vector<uint32>> SlotTriangles;
	std::vector<FVertexSource> Sources;
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

	// 11. 가중치: 뼈 중심 → 정점 중심 (F)
	if (!OutData.Bones.IsEmpty())
	{
		FillSkinWeights(MeshNodes, Sources, BoneIndexByNode, OutData.SkinWeights, FbxPath);
	}

	// 12. 바인드 포즈 → InverseBindPose (G)
	FillBindPoses(MeshNodes, BoneIndexByNode, Axis, OutData.Bones);
	if (const int32 Outside = CountBonesOutsideMesh(OutData); Outside > 0)
	{
		HTR_LOG(Warning, "[FBX] {}: {} bones are outside the mesh bounds (axis or bind matrix mismatch?)", FbxPath, Outside);
	}

	// 13. 검사
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

	// 14. 정리: 스코프 종료 시 Manager Destroy (Scene·Node 포함)
}

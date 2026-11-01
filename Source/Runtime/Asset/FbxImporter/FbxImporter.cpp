#include "EnginePCH.h"
#include "FbxImporter.h"

#include <fbxsdk.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <utility>

#include "Core/EngineLog.h"
#include "Render/SkeletalMeshData.h"

namespace
{
	struct FFbxContext
	{
		FbxManager* Manager = nullptr;
		FbxScene* Scene = nullptr;

		~FFbxContext()
		{
			if (Manager)
			{
				Manager->Destroy();
			}
		}
	};

	const char* GetAttributeTypeName(FbxNode* Node)
	{
		const FbxNodeAttribute* Attribute = Node->GetNodeAttribute();
		if (!Attribute)
		{
			return "None";
		}

		switch (Attribute->GetAttributeType())
		{
		case FbxNodeAttribute::eNull:     return "Null";
		case FbxNodeAttribute::eSkeleton: return "Skeleton";
		case FbxNodeAttribute::eMesh:     return "Mesh";
		case FbxNodeAttribute::eCamera:   return "Camera";
		case FbxNodeAttribute::eLight:    return "Light";
		case FbxNodeAttribute::eMarker:   return "Marker";
		default:                          return "Other";
		}
	}

	// 디버그용: 노드 트리를 깊이만큼 들여쓰기해서 출력한다
	void PrintNodeTree(FbxNode* Node, int32 Depth)
	{
		const FbxMesh* Mesh = Node->GetMesh();
		HTR_LOG(Info, "{}{} [{}]{}", FString(static_cast<size_t>(Depth) * 2, ' '), Node->GetName(), GetAttributeTypeName(Node), Mesh && !Mesh->IsTriangleMesh() ? " (NOT TRIANGLE)" : "");

		for (int32 i = 0; i < Node->GetChildCount(); ++i)
		{
			PrintNodeTree(Node->GetChild(i), Depth + 1);
		}
	}

	// 재귀적으로 특정 노드에서 시작하여 자식 노드들을 dfs로 탐색하여 Bone을 모은다.
	void CollectBones(FbxNode* Node, int32 ParentIndex, TArray<FBoneInfo>& OutBones, TArray<FbxNode*>& OutBoneNodes)
	{
		const FbxNodeAttribute* Attribute = Node->GetNodeAttribute();
		if (Attribute && Attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton)
		{
			FBoneInfo Bone;
			Bone.Name = Node->GetName();
			Bone.ParentIndex = ParentIndex;

			ParentIndex = OutBones.Num();
			OutBones.Add(Bone);
			OutBoneNodes.Add(Node);
		}

		for (int32 i = 0; i < Node->GetChildCount(); ++i)
		{
			CollectBones(Node->GetChild(i), ParentIndex, OutBones, OutBoneNodes);
		}
	}
	struct FFbxVertexKey
	{
		int32 CP = -1;
		float NX = 0.0f, NY = 0.0f, NZ = 0.0f;
		float U = 0.0f, V = 0.0f;

		bool operator==(const FFbxVertexKey&) const = default;
	};

	// 키 해시
	struct FFbxVertexKeyHash
	{
		size_t operator()(const FFbxVertexKey& Key) const
		{
			size_t Hash = std::hash<int32>{}(Key.CP);
			for (float F : { Key.NX, Key.NY, Key.NZ, Key.U, Key.V })
			{
				Hash = Hash * 31 + std::hash<float>{}(F);
			}
			return Hash;
		}
	};
	void CollectMeshNodes(FbxNode* Node, TArray<FbxNode*>& OutMeshNodes)
	{
		const FbxNodeAttribute* Attribute = Node->GetNodeAttribute();
		if (Attribute && Attribute->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			OutMeshNodes.Add(Node);
		}
		for (int32 i = 0; i < Node->GetChildCount(); ++i)
		{
			CollectMeshNodes(Node->GetChild(i), OutMeshNodes);
		}
	}
	// 오른손 좌표계를 엔진의 왼손 좌표계로 변환한다. 
	FMatrix ToEngineMatrix(const FbxMatrix& Source)
	{
		FMatrix Out;
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Col = 0; Col < 4; ++Col)
			{
				Out.M[Row][Col] = static_cast<float>(Source.Get(Row, Col));
			}
		}
		for (int32 i = 0; i < 4; ++i)
		{
			Out.M[1][i] = -Out.M[1][i];
			Out.M[i][1] = -Out.M[i][1];
		}
		return Out;
	}

	// 각 Bone의 Global 행렬을 구한 뒤, 부모 기준의 Local 행렬로 바꿔 RefLocal에 저장한다.
	void ExtractRefLocals(TArray<FBoneInfo>& Bones, const TArray<FbxNode*>& BoneNodes)
	{
		TArray<FMatrix> Globals;
		for (int32 i = 0; i < Bones.Num(); ++i)
		{
			const FMatrix Global = ToEngineMatrix(BoneNodes[i]->EvaluateGlobalTransform());
			Globals.Add(Global);

			const int32 Parent = Bones[i].ParentIndex;
			Bones[i].RefLocal = Parent >= 0 ? Global * Globals[Parent].Inverse() : Global;
			// 기본값: Cluster가 없는 뼈는 기본 자세를 바인드 자세로 사용
			Bones[i].InvBindPose = Global.Inverse();
		}
	}

	// 디버그용: Scene의 축계·단위를 로그로 남긴다
	void LogSceneSpace(const char* Label, FbxScene* Scene)
	{
		FbxGlobalSettings& Settings = Scene->GetGlobalSettings();
		const FbxAxisSystem Axis = Settings.GetAxisSystem();

		int UpSign = 0, FrontSign = 0;
		const FbxAxisSystem::EUpVector Up = Axis.GetUpVector(UpSign);
		const FbxAxisSystem::EFrontVector Front = Axis.GetFrontVector(FrontSign);
		const bool bRightHanded = Axis.GetCoorSystem() == FbxAxisSystem::eRightHanded;

		HTR_LOG(Info, "FBX space [{}]: Up={}{}, FrontParity={}{}, {}, Unit={}cm",
			Label,
			UpSign > 0 ? "+" : "-", "?XYZ"[Up],
			FrontSign > 0 ? "+" : "-", Front == FbxAxisSystem::eParityEven ? "Even" : "Odd",
			bRightHanded ? "RH" : "LH",
			Settings.GetSystemUnit().GetScaleFactor());
	}

	// 여기서는 오른손 Z-Up 좌표계 변환만 한다. 왼손 좌표계로의 변환은 나중에
	void ConvertToEngineSpace(FbxScene* Scene, FbxManager* Manager)
	{
		LogSceneSpace("before", Scene);

		// AxisSystem 설정값 선언
		const FbxAxisSystem TargetAxis(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityEven, FbxAxisSystem::eRightHanded);
		if (Scene->GetGlobalSettings().GetAxisSystem() != TargetAxis)
		{
			// AxisSystem이 다르면 변환
			TargetAxis.ConvertScene(Scene);
		}

		if (Scene->GetGlobalSettings().GetSystemUnit() != FbxSystemUnit::m)
		{
			// 길이 단위가 미터가 아닌 경우 미터로 변환
			FbxSystemUnit::m.ConvertScene(Scene);
		}

		// 모든 폴리곤을 삼각형으로 쪼갠다
		FbxGeometryConverter Converter(Manager);
		if (!Converter.Triangulate(Scene, true))
		{
			HTR_LOG(Warning, "FBX triangulate failed on some meshes");
		}

		LogSceneSpace("after", Scene);
	}

	// Context의 Scene을 파일을 읽어서 로드. 축변환, 삼각형화까지 한다.
	bool LoadScene(const FString& Path, FFbxContext& Context)
	{
		Context.Manager = FbxManager::Create();

		FbxIOSettings* IOSettings = FbxIOSettings::Create(Context.Manager, IOSROOT);
		Context.Manager->SetIOSettings(IOSettings);

		FbxImporter* Importer = FbxImporter::Create(Context.Manager, "");
		if (!Importer->Initialize(Path.c_str(), -1, IOSettings))
		{
			// 파일 열기 실패
			HTR_LOG(Error, "FBX open failed: {} ({})", Path, Importer->GetStatus().GetErrorString());
			Importer->Destroy();
			return false;
		}

		Context.Scene = FbxScene::Create(Context.Manager, "ImportScene");
		// 파일을 읽어 Scene에 채운다
		const bool bImported = Importer->Import(Context.Scene);
		if (!bImported)
		{
			// import 실패
			HTR_LOG(Error, "FBX import failed: {} ({})", Path, Importer->GetStatus().GetErrorString());
		}

		Importer->Destroy();

		if (bImported)
		{
			ConvertToEngineSpace(Context.Scene, Context.Manager);
		}

		return bImported;
	}
	// 머티리얼 속성에 연결된 텍스처 파일 경로 찾기 (Step 7)
	FString FindTexturePath(FbxSurfaceMaterial* Material, const char* PropertyName, const FString& FbxPath)
	{
		const FbxProperty Property = Material->FindProperty(PropertyName);
		if (!Property.IsValid())
		{
			return FString();
		}
		const FbxFileTexture* Texture = Property.GetSrcObject<FbxFileTexture>(0);
		if (!Texture)
		{
			return FString();
		}

		namespace fs = std::filesystem;
		const fs::path Dir = fs::path(FbxPath).parent_path();
		const fs::path FileName = fs::path(Texture->GetFileName()).filename();
		const fs::path Candidates[] =
		{
			Dir / Texture->GetRelativeFileName(),  // 1. FBX 기준 상대 경로
			fs::path(Texture->GetFileName()),      // 2. 저장 당시 절대 경로
			Dir / FileName,                        // 3. FBX와 같은 폴더
			Dir / "textures" / FileName,           // 4. textures 하위 폴더
		};
		for (const fs::path& Candidate : Candidates)
		{
			std::error_code Error;
			if (fs::is_regular_file(Candidate, Error))
			{
				return Candidate.lexically_normal().generic_string();
			}
		}
		HTR_LOG(Warning, "FBX texture not found: {}", Texture->GetFileName());
		return FString();
	}

	void ExtractMesh(FbxScene* Scene, const FString& Path, FStaticMeshData& MeshData)
	{
		// 1. 메시 노드 수집
		TArray<FbxNode*> MeshNodes;
		CollectMeshNodes(Scene->GetRootNode(), MeshNodes);

		// 2. 노드별 정점·인덱스 생성
		int32 CPOffset = 0;
		TMap<FFbxVertexKey, uint32, FFbxVertexKeyHash> VertexCache;
		TArray<TArray<uint32>> SlotIndices;
		for (FbxNode* Node : MeshNodes)
		{
			FbxMesh* Mesh = Node->GetMesh();
			const FbxAMatrix Global = Node->EvaluateGlobalTransform();
			FbxAMatrix NormalMatrix = Global;
			NormalMatrix.SetT(FbxVector4(0.0, 0.0, 0.0, 0.0)); // NormalMatrix에서 이동에 대한 성분을 뺌

			FbxStringList UVSetNames;
			Mesh->GetUVSetNames(UVSetNames);
			const char* UVSetName = UVSetNames.GetCount() > 0 ? UVSetNames[0].Buffer() : nullptr;
			
			// 머티리얼 슬롯 등록
			const int32 SlotOffset = MeshData.MaterialSlots.Num();
			const int32 MaterialCount = Node->GetMaterialCount();

			for (int32 m = 0;m < MaterialCount; ++m)
			{
				FStaticMaterialSlot Slot;
				//Material은 메시가 아니라 노드에 붙어있음
				FbxSurfaceMaterial* Material = Node->GetMaterial(m);
				Slot.Name = Material->GetName();
				// 텍스처 경로 (Step 7)
				Slot.DiffuseTexturePath = FindTexturePath(Material, FbxSurfaceMaterial::sDiffuse, Path);
				Slot.NormalTexturePath = FindTexturePath(Material, FbxSurfaceMaterial::sNormalMap, Path);
				if (Slot.NormalTexturePath.empty())
				{
					Slot.NormalTexturePath = FindTexturePath(Material, FbxSurfaceMaterial::sBump, Path);
				}
				MeshData.MaterialSlots.Add(Slot);
			}
			//Material이 없는 노드의 경우
			if (MaterialCount == 0)
			{
				FStaticMaterialSlot Slot;
				Slot.Name = "Default";
				MeshData.MaterialSlots.Add(Slot);
			}
			SlotIndices.SetNum(MeshData.MaterialSlots.Num());
			FbxGeometryElementMaterial* MaterialElement = Mesh->GetElementMaterial();

			for (int32 p = 0; p < Mesh->GetPolygonCount(); ++p)
			{
				// 2-1. 삼각형 아닌 폴리곤 제외
				if (Mesh->GetPolygonSize(p) != 3)
				{
					continue;
				}

				// 2-2. 코너 3개 → 정점 3개
				uint32 Corner[3];
				for (int32 c = 0; c < 3; ++c)
				{
					//p번 삼각형의 c번째 코너(a,b,c)는 몇번 control Point인가?
					const int32 CP = Mesh->GetPolygonVertex(p, c);
					const FbxVector4 P = Global.MultT(Mesh->GetControlPointAt(CP));

					FVertex Vertex;
					Vertex.Position = FVector(static_cast<float>(P[0]), -static_cast<float>(P[1]), static_cast<float>(P[2]));
					
					//Normal 추출
					Vertex.Normal = FVector(0.0f, 0.0f, 1.0f);
					FbxVector4 N;
					// p번 삼각형의 c번째 코너의 노멀을 N에 받음.
					if (Mesh->GetPolygonVertexNormal(p, c, N))
					{
						N = NormalMatrix.MultT(N); //노드의 회전을 반영해준 노말값이됨
						Vertex.Normal = FVector(static_cast<float>(N[0]), -static_cast<float>(N[1]), static_cast<float>(N[2]))
							.GetSafeNormal(SMALL_NUMBER, FVector(0.0f, 0.0f, 1.0f));

					}
					FbxVector2 UV;
					bool bUnmapped = false;
					if (UVSetName && Mesh->GetPolygonVertexUV(p, c, UVSetName, UV, bUnmapped) && !bUnmapped)
					{
						Vertex.UV.X = static_cast<float>(UV[0]);
						Vertex.UV.Y = 1.0f - static_cast<float>(UV[1]);
						// FBX는 텍스처 이미지의 왼쪽 아래가 원점, D3D는 왼쪽 위가 원점
					}

					// 2-3. 같은 정점이면 재사용, 없으면 추가
					const FFbxVertexKey Key{ CPOffset + CP, Vertex.Normal.X, Vertex.Normal.Y, Vertex.Normal.Z, Vertex.UV.X, Vertex.UV.Y };
					if (const uint32* Found = VertexCache.Find(Key))
					{
						Corner[c] = *Found;
					}
					else
					{
						Corner[c] = static_cast<uint32>(MeshData.Vertices.Num());
						VertexCache.Add(Key, Corner[c]);
						MeshData.Vertices.Add(Vertex);
						MeshData.LODSourceVertices.Add({ CPOffset + CP, -1, -1 });
					}
				}
				// 2-4. 이 삼각형의 슬롯 바구니에 인덱스 추가
				int32 LocalSlot = 0;
				if (MaterialElement)
				{
					const int32 Lookup = MaterialElement->GetMappingMode() == FbxGeometryElement::eByPolygon ? p : 0;
					LocalSlot = MaterialElement->GetIndexArray().GetAt(Lookup);
				}
				if (LocalSlot < 0 || LocalSlot >= std::max(MaterialCount, 1))
				{
					LocalSlot = 0;
				}
				TArray<uint32>& Bucket = SlotIndices[SlotOffset + LocalSlot];
				// 인덱스(FBX는 반시계기준.엔진에 맞게 시계방향으로 보정)
				Bucket.Add(Corner[0]);
				Bucket.Add(Corner[2]);
				Bucket.Add(Corner[1]);
			}

			CPOffset += Mesh->GetControlPointsCount();
		}

		// 3. 바구니 → Section
		for (int32 s = 0; s < SlotIndices.Num(); ++s)
		{
			if (SlotIndices[s].IsEmpty())
			{
				continue;
			}
			FStaticMeshSection Section;
			Section.StartIndex = static_cast<uint32>(MeshData.Indices.Num());
			Section.IndexCount = static_cast<uint32>(SlotIndices[s].Num());
			Section.MaterialSlotIndex = static_cast<uint32>(s);
			MeshData.Indices.Append(SlotIndices[s]);
			MeshData.Sections.Add(Section);
		}

		// 4. AABB
		if (!MeshData.Vertices.IsEmpty())
		{
			FVector Min = MeshData.Vertices[0].Position;
			FVector Max = Min;
			for (const FVertex& Vertex : MeshData.Vertices)
			{
				Min.X = std::min(Min.X, Vertex.Position.X);
				Min.Y = std::min(Min.Y, Vertex.Position.Y);
				Min.Z = std::min(Min.Z, Vertex.Position.Z);
				Max.X = std::max(Max.X, Vertex.Position.X);
				Max.Y = std::max(Max.Y, Vertex.Position.Y);
				Max.Z = std::max(Max.Z, Vertex.Position.Z);
			}
			MeshData.AABB.Min = Min;
			MeshData.AABB.Max = Max;
		}

		// 5. 결과 로그
		HTR_LOG(Info, "FBX mesh: nodes {}, verts {}, tris {}, sections {}",
			MeshNodes.Num(), MeshData.Vertices.Num(), MeshData.Indices.Num() / 3, MeshData.Sections.Num());

		// 6. 상자 검사
		FString Error;
		if (!MeshData.Validate(Error))
		{
			HTR_LOG(Error, "FBX mesh invalid: {}", Error);
		}
	}

	// Skin(가중치) + InvBindPose 추출 (Step 6)
	void ExtractSkin(FbxScene* Scene, const TArray<FbxNode*>& BoneNodes, FSkeletalMeshData& Data)
	{
		if (Data.Bones.IsEmpty() || Data.Mesh.Vertices.IsEmpty())
		{
			return;
		}

		// 1. 뼈 노드 → 뼈 번호
		TMap<FbxNode*, int32> BoneIndexOf;
		for (int32 i = 0; i < BoneNodes.Num(); ++i)
		{
			BoneIndexOf.Add(BoneNodes[i], i);
		}

		// 2. 기본 자세 Global (바인드 자세와 비교용)
		TArray<FMatrix> RefGlobals;
		for (int32 i = 0; i < Data.Bones.Num(); ++i)
		{
			const int32 Parent = Data.Bones[i].ParentIndex;
			RefGlobals.Add(Parent >= 0 ? Data.Bones[i].RefLocal * RefGlobals[Parent] : Data.Bones[i].RefLocal);
		}

		// 3. Cluster 순회: Control Point별 (뼈, 비율) 수집
		TArray<FbxNode*> MeshNodes;
		CollectMeshNodes(Scene->GetRootNode(), MeshNodes);

		TArray<TArray<std::pair<int32, float>>> CPInfluences;
		int32 CPOffset = 0;
		int32 ClusterCount = 0;
		float MaxBindDiff = 0.0f;
		for (FbxNode* Node : MeshNodes)
		{
			FbxMesh* Mesh = Node->GetMesh();
			CPInfluences.SetNum(CPOffset + Mesh->GetControlPointsCount());

			for (int32 d = 0; d < Mesh->GetDeformerCount(FbxDeformer::eSkin); ++d)
			{
				FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(d, FbxDeformer::eSkin));
				for (int32 c = 0; c < Skin->GetClusterCount(); ++c)
				{
					FbxCluster* Cluster = Skin->GetCluster(c);
					const int32* BoneFound = BoneIndexOf.Find(Cluster->GetLink());
					if (!BoneFound)
					{
						continue;
					}
					const int32 Bone = *BoneFound;
					++ClusterCount;

					// 3-1. InvBindPose = 바인드 순간 뼈 행렬의 역
					FbxAMatrix BindMatrix;
					Cluster->GetTransformLinkMatrix(BindMatrix);
					const FMatrix Bind = ToEngineMatrix(BindMatrix);
					Data.Bones[Bone].InvBindPose = Bind.Inverse();

					// 3-2. 바인드 자세 vs 기본 자세 차이 (T-Pose 검증용)
					for (int32 r = 0; r < 4; ++r)
					{
						for (int32 k = 0; k < 4; ++k)
						{
							MaxBindDiff = std::max(MaxBindDiff, std::abs(Bind.M[r][k] - RefGlobals[Bone].M[r][k]));
						}
					}

					// 3-3. 이 뼈가 끄는 Control Point와 비율
					const int32 Count = Cluster->GetControlPointIndicesCount();
					const int* Indices = Cluster->GetControlPointIndices();
					const double* Weights = Cluster->GetControlPointWeights();
					for (int32 k = 0; k < Count; ++k)
					{
						const int32 CP = CPOffset + Indices[k];
						if (Weights[k] > 0.0 && CP < CPInfluences.Num())
						{
							CPInfluences[CP].Add({ Bone, static_cast<float>(Weights[k]) });
						}
					}
				}
			}

			CPOffset += Mesh->GetControlPointsCount();
		}

		// 4. Control Point별 상위 4개 + 정규화
		TArray<FSkinWeight> CPWeights;
		CPWeights.SetNum(CPInfluences.Num());
		int32 Unweighted = 0;
		for (int32 cp = 0; cp < CPInfluences.Num(); ++cp)
		{
			TArray<std::pair<int32, float>>& List = CPInfluences[cp];
			std::sort(List.begin(), List.end(), [](const std::pair<int32, float>& A, const std::pair<int32, float>& B) { return A.second > B.second; });

			const int32 Keep = std::min(List.Num(), 4);
			float Sum = 0.0f;
			for (int32 k = 0; k < Keep; ++k)
			{
				Sum += List[k].second;
			}

			FSkinWeight& Out = CPWeights[cp];
			if (Sum <= 0.0f)
			{
				Out.Weights[0] = 1.0f; // 영향 없는 점은 0번(루트) 뼈를 따라감
				++Unweighted;
				continue;
			}
			for (int32 k = 0; k < Keep; ++k)
			{
				Out.BoneIndices[k] = List[k].first;
				Out.Weights[k] = List[k].second / Sum;
			}
		}

		// 5. 정점마다 원래 Control Point의 가중치 복사
		Data.SkinWeights.SetNum(Data.Mesh.Vertices.Num());
		for (int32 v = 0; v < Data.Mesh.Vertices.Num(); ++v)
		{
			const int32 CP = Data.Mesh.LODSourceVertices[v].PositionIndex;
			if (CP >= 0 && CP < CPWeights.Num())
			{
				Data.SkinWeights[v] = CPWeights[CP];
			}
		}

		// 6. 결과 로그
		HTR_LOG(Info, "FBX skin: clusters {}, unweighted CPs {}, bind-vs-ref max diff {}", ClusterCount, Unweighted, MaxBindDiff);
	}
}


TUniquePtr<FSkeletalMeshData> FFbxImporter::LoadSkeletalMeshData(const FString& Path)
{
	FFbxContext Context;

	// 1. 파일열기
	if (!LoadScene(Path, Context))
	{
		return nullptr;
	}

	TUniquePtr<FSkeletalMeshData> Data = MakeUnique<FSkeletalMeshData>();
	TArray<FbxNode*> BoneNodes;
	CollectBones(Context.Scene->GetRootNode(), -1, Data->Bones, BoneNodes);
	if (Data->Bones.IsEmpty())
	{
		HTR_LOG(Warning, "FBX has no skeleton: {}", Path);
	}
	ExtractRefLocals(Data->Bones, BoneNodes);
	ExtractMesh(Context.Scene, Path, Data->Mesh);
	ExtractSkin(Context.Scene, BoneNodes, *Data);
	return Data;
}

// 디버그용 출력
void FFbxImporter::PrintFbxInfo(const FString& Path)
{
	FFbxContext Context;
	if (!LoadScene(Path, Context))
	{
		return;
	}

	HTR_LOG(Info, "FBX loaded: {}", Path);
	HTR_LOG(Info, "SDK Version: {}", Context.Manager->GetVersion());
	HTR_LOG(Info, "Root Children: {}", Context.Scene->GetRootNode()->GetChildCount());
	HTR_LOG(Info, "Node Count: {}", Context.Scene->GetNodeCount());
	int32 NonTriangleMeshes = 0;
	const int32 MeshCount = Context.Scene->GetSrcObjectCount<FbxMesh>();
	for (int32 i = 0; i < MeshCount; ++i)
	{
		if (!Context.Scene->GetSrcObject<FbxMesh>(i)->IsTriangleMesh())
		{
			++NonTriangleMeshes;
		}
	}
	HTR_LOG(Info, "Meshes: {} (non-triangle: {})", MeshCount, NonTriangleMeshes);

	PrintNodeTree(Context.Scene->GetRootNode(), 0);
}

void FFbxImporter::PrintSkeleton(const FSkeletalMeshData& Data)
{
	HTR_LOG(Info, "Bones: {}", Data.Bones.Num());

	TArray<int32> Depths;
	for (int32 i = 0; i < Data.Bones.Num(); ++i)
	{
		const FBoneInfo& Bone = Data.Bones[i];
		const int32 Depth = Bone.ParentIndex >= 0 ? Depths[Bone.ParentIndex] + 1 : 0;
		Depths.Add(Depth);

		HTR_LOG(Info, "{}[{}] {}", FString(static_cast<size_t>(Depth) * 2, ' '), i, Bone.Name);
	}
}
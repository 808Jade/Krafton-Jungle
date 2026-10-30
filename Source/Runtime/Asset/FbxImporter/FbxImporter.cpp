#include "EnginePCH.h"
#include "FbxImporter.h"

#include <fbxsdk.h>

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

	TArray<FBoneInfo> Bones;
	TArray<FbxNode*> BoneNodes;
	CollectBones(Context.Scene->GetRootNode(), -1, Bones, BoneNodes);

	HTR_LOG(Info, "Bones: {}", Bones.Num());
	for (int32 i = 0; i < Bones.Num(); ++i)
	{
		HTR_LOG(Info, "[{}] {} (parent {})", i, Bones[i].Name, Bones[i].ParentIndex);
	}
}
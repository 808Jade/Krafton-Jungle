#include "EnginePCH.h"
#include "FbxImporter.h"

#include <fbxsdk.h>

#include "Core/EngineLog.h"

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

	// FBX SDK는 좌표계 변환을 해주지 않는다. 여기서는 오른손 Z-Up까지만 변환을 한다. 왼손 좌표계로의 변환은 나중에
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
}
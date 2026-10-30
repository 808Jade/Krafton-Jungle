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
}
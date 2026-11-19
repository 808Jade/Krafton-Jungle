#include "EnginePCH.h"
#include "Asset/AssetManager.h"

#include "Core/Misc/FileHelper.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleSystem.h"
#include "Particles/Modules/ParticleModuleRequired.h"
#include "Particles/Modules/ParticleModuleTypeDataBase.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/StructuredArchive.h"
#include "Serialization/Formatter/JsonArchiveInputFormatter.h"
#include "Serialization/Formatter/JsonArchiveOutputFormatter.h"
#include "UObject/Casts.h"
#include <algorithm>

namespace
{
	// 파일 형식이 바뀔 때만 올린다. 프로퍼티를 추가하는 것만으로는 올릴 필요가 없다.
	constexpr int32 ParticleSystemVersion = 1;
	constexpr const char* ParticleSystemType = "ParticleSystem";

	bool ResolveParticleSystemPath(const FString& Path, fs::path& Native, FString& Key, FString& Error)
	{
		std::error_code EC;
		Native = fs::weakly_canonical(FFileHelper::ToNativePath(Path), EC);
		if (EC) { Error = "Cannot resolve asset path."; return false; }
		const fs::path Root = fs::weakly_canonical("Assets", EC);
		if (EC) { Error = "Cannot resolve Assets folder."; return false; }
		const fs::path Relative = Native.lexically_relative(Root);
		if (Relative.empty() || Relative.is_absolute() || *Relative.begin() == ".." ||
			_stricmp(Native.extension().string().c_str(), ".particle") != 0)
		{
			Error = "Choose a .particle file inside Assets."; return false;
		}
		Key = FFileHelper::ToUtf8Path(Relative);
		return true;
	}

	// 불러오다 실패한 에셋을 정리한다. 하위 객체는 소유자(Outer)가 지우지 않으므로 직접 지운다.
	void DestroyParticleSystemTree(UParticleSystem* System)
	{
		for (UParticleEmitter* Emitter : System->Emitters)
		{
			if (!Emitter)
			{
				continue;
			}
			for (UParticleLODLevel* LODLevel : Emitter->LODLevels)
			{
				if (!LODLevel)
				{
					continue;
				}
				for (UParticleModule* Module : LODLevel->Modules)
				{
					delete Module;
				}
				delete LODLevel->RequiredModule;
				delete LODLevel->TypeDataModule;
				delete LODLevel;
			}
			delete Emitter;
		}
		delete System;
	}

	// 경로가 없는 머티리얼은 파일에 null로 저장되어 돌아오지 않는다. 조용히 사라지지 않게 알린다.
	void WarnUnsavableReferences(const UParticleSystem& System)
	{
		for (const UParticleEmitter* Emitter : System.Emitters)
		{
			if (!Emitter)
			{
				continue;
			}
			for (const UParticleLODLevel* LODLevel : Emitter->LODLevels)
			{
				const UParticleModuleRequired* Required = LODLevel ? LODLevel->RequiredModule : nullptr;
				const URenderAsset* Material = Required ? Cast<URenderAsset>(Required->Material) : nullptr;
				if (Material && Material->GetPath().empty())
				{
					HTR_LOG(Warning, "[ParticleSystem] Required material has no asset key; it will be saved as null.");
				}
			}
		}
	}
}

UParticleSystem* UAssetManager::LoadParticleSystem(const FString& Path, FString& Error)
{
	Error.clear();
	fs::path Native;
	FString Key;
	if (!ResolveParticleSystemPath(Path, Native, Key, Error)) return nullptr;
	if (auto* Existing = GetAssetByPath<UParticleSystem>(Key)) return Existing;
	if (AssetMap.Contains(Key)) { Error = "Asset key already belongs to another asset type."; return nullptr; }

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, FFileHelper::ToUtf8Path(Native)))
	{
		Error = "Cannot read ParticleSystem file."; return nullptr;
	}

	FMemoryReader Reader(Bytes);
	FJsonArchiveInputFormatter Formatter(Reader);
	if (Reader.IsError()) { Error = "Invalid ParticleSystem file (not valid JSON)."; return nullptr; }

	UParticleSystem* System = nullptr;
	{
		FStructuredArchive Ar(Formatter);
		FStructuredArchive::FRecord Root = Ar.Open().EnterRecord();

		int32 Version = 0;
		FString Type;
		Root << SA_VALUE("Version", Version) << SA_VALUE("Type", Type);
		if (Version != ParticleSystemVersion || Type != ParticleSystemType)
		{
			Error = "Unsupported ParticleSystem version or type."; return nullptr;
		}

		System = NewObject<UParticleSystem>();
		System->Serialize(Root.EnterRecord("System"));
		Ar.Close();
	}

	if (Reader.IsError())
	{
		DestroyParticleSystemTree(System);
		Error = "Failed to read ParticleSystem data."; return nullptr;
	}

	// 컴포넌트가 InitParticles에서 다시 만들지만, 에디터 패널이 먼저 읽을 수 있으므로 여기서도 만든다.
	for (UParticleEmitter* Emitter : System->Emitters)
	{
		Emitter->CacheEmitterModuleInfo();
	}

	RegisterAsset(Key, System);
	AssetPathMap.Add(Key, FFileHelper::ToUtf8Path(Native));
	return System;
}

bool UAssetManager::SaveParticleSystem(const FString& Path, UParticleSystem* System, FString& Error, bool bCreateOnly)
{
	Error.clear();
	if (!System) { Error = "No particle system to save."; return false; }

	fs::path Native;
	FString Key;
	if (!ResolveParticleSystemPath(Path, Native, Key, Error)) return false;

	std::error_code EC;
	const bool bExists = fs::exists(Native, EC);
	if (EC) { Error = "Cannot inspect destination file."; return false; }
	if (bCreateOnly && (bExists || AssetMap.Contains(Key))) { Error = "An asset with that name already exists."; return false; }

	// 이미 다른 객체가 이 키로 등록돼 있으면 파일만 바뀌고 메모리 에셋은 낡은 채로 남는다.
	URenderAsset** Registered = AssetMap.Find(Key);
	if (Registered && *Registered != System)
	{
		Error = "Another asset is already registered under this name."; return false;
	}

	WarnUnsavableReferences(*System);

	TArray<uint8> Bytes;
	{
		FMemoryWriter Writer(Bytes);
		{
			// 선언 순서 = 소멸 역순: Ar(Close) -> Formatter(dump -> Writer)
			FJsonArchiveOutputFormatter Formatter(Writer);
			FStructuredArchive Ar(Formatter);

			FStructuredArchive::FRecord Root = Ar.Open().EnterRecord();
			int32 Version = ParticleSystemVersion;
			FString Type = ParticleSystemType;
			Root << SA_VALUE("Version", Version) << SA_VALUE("Type", Type);
			System->Serialize(Root.EnterRecord("System"));
			Ar.Close();
		}
		if (Writer.IsError()) { Error = "Cannot encode asset data."; return false; }
	}

	fs::create_directories(Native.parent_path(), EC);
	if (EC) { Error = "Cannot create the destination folder."; return false; }

	// 쓰다 실패해도 마지막으로 저장된 파일이 남도록, 같은 폴더의 임시 파일에 쓴 뒤 바꿔치기한다.
	wchar_t TempName[MAX_PATH];
	if (!GetTempFileNameW(Native.parent_path().c_str(), L"par", 0, TempName))
	{
		Error = "Cannot create temporary save file."; return false;
	}
	const fs::path Temp(TempName);
	bool Saved = FFileHelper::SaveArrayToFile(Bytes, FFileHelper::ToUtf8Path(Temp));
	if (Saved)
	{
		Saved = MoveFileExW(Temp.c_str(), Native.c_str(), MOVEFILE_WRITE_THROUGH |
			(bCreateOnly ? 0 : MOVEFILE_REPLACE_EXISTING)) != FALSE;
	}
	if (!Saved)
	{
		fs::remove(Temp, EC);
		Error = "Cannot save or replace asset file. Check file permissions.";
		return false;
	}

	AssetPathMap.Add(Key, FFileHelper::ToUtf8Path(Native));
	return true;
}

TArray<UParticleSystem*> UAssetManager::GetParticleSystemAssets() const
{
	TArray<UParticleSystem*> Result;
	for (const auto& Entry : AssetMap)
	{
		if (Entry.second && Entry.second->IsA<UParticleSystem>())
		{
			Result.Add(Cast<UParticleSystem>(Entry.second));
		}
	}
	std::sort(Result.begin(), Result.end(), [](const auto* A, const auto* B) { return A->GetPath() < B->GetPath(); });
	return Result;
}

void UAssetManager::LoadParticleSystemAssets(const fs::path& AssetRoot)
{
	std::error_code EC;
	if (!fs::exists(AssetRoot, EC))
	{
		return;
	}

	TArray<fs::path> Files;
	for (const fs::directory_entry& Entry : fs::recursive_directory_iterator(AssetRoot, EC))
	{
		if (Entry.is_regular_file() && _stricmp(Entry.path().extension().string().c_str(), ".particle") == 0)
		{
			Files.Add(Entry.path());
		}
	}
	// 폴더 순회 순서는 보장되지 않으므로 이름순으로 고정한다.
	std::sort(Files.begin(), Files.end());

	for (const fs::path& File : Files)
	{
		FString Error;
		if (!LoadParticleSystem(FFileHelper::ToUtf8Path(File), Error))
		{
			HTR_LOG(Error, "[ParticleSystem] {}: {}", FFileHelper::ToUtf8Path(File), Error);
		}
	}
}

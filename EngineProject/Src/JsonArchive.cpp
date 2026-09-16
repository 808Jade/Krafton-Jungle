#include "EnginePCH.h"
#include "JsonArchive.h"

#include "Engine/World.h"
#include "Engine/Level.h"
#include "Core/EngineStatics.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Actor/StaticMeshActor.h"

namespace
{
	FString PrimitiveTypeToString(EPrimitiveType Type)
	{
		switch (Type)
		{
		case EPrimitiveType::Sphere:
			return "Sphere";

		case EPrimitiveType::Cube:
			return "Cube";

		case EPrimitiveType::Cone:
			return "Cone";

		case EPrimitiveType::Plane:
			return "Plane";

		default:
			return "";
		}
	}

	EPrimitiveType FStringToPrimitiveType(const FString& String)
	{
		if (String == "Sphere")
			return EPrimitiveType::Sphere;

		if (String == "Cube")
			return EPrimitiveType::Cube;

		if (String == "Cone")
			return EPrimitiveType::Cone;

		if (String == "Plane")
			return EPrimitiveType::Plane;

		return EPrimitiveType::Cube;
	}

}

bool FJsonArchive::SaveWorld(UWorld* World, const FString& Path)
{
	if (!World)
		return false;

	ULevel* Level = World->GetPersistentLevel();

	if (!Level)
		return false;

	json Json;

	Json["Version"] = 1;
	Json["NextUUID"] = FEngineStatics::NextUUID;
	Json["Primitives"] = json::object();

	for (AActor* Actor : Level->GetActors())
	{
		if (!Actor)
			continue;

		USceneComponent* RootComponent = Actor->GetRootComponent();

		if (!RootComponent)
			continue;

		json PrimitiveJson;

		const FTransform& Transform = RootComponent->GetTransform();

		PrimitiveJson["Location"] =
		{
			Transform.Location.X,
			Transform.Location.Y,
			Transform.Location.Z
		};

		PrimitiveJson["Rotation"] =
		{
			Transform.Rotation.Roll,
			Transform.Rotation.Pitch,
			Transform.Rotation.Yaw
		};

		PrimitiveJson["Scale"] =
		{
			Transform.Scale.X,
			Transform.Scale.Y,
			Transform.Scale.Z
		};

		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(RootComponent))
		{
			PrimitiveJson["Type"] = PrimitiveTypeToString(Primitive->GetType());
		}
		else
		{
			PrimitiveJson["Type"] = "Other";
		}

		Json["Primitives"][std::to_string(Actor->GetUUID())] = PrimitiveJson;
	}

	std::ofstream File(Path);

	if (!File.is_open())
		return false;

	File << Json.dump(4);

	File.close();

	return true;
}

bool FJsonArchive::LoadWorld(UWorld* World, const FString& Path)
{
	if (!World)
		return false;

	if (!std::filesystem::exists(Path))
	{
		LOG(Warning, "{} is Not Exist!", Path);
		return false;
	}

	std::ifstream File(Path);

	if (!File.is_open())
		return false;

	json Json;

	try
	{
		File >> Json;
	}
	catch (const json::parse_error&)
	{
		return false;
	}

	// 여기까지 성공해야 기존 Scene을 건드릴 수 있음
	if (!Json.contains("Version"))
		return false;

	if (Json["Version"] != 1)
		return false;

	if (!Json.contains("NextUUID"))
		return false;

	// 파일 검증이 끝난 뒤 Clear
	World->ClearScene();

	uint64 SavedNextUUID = Json["NextUUID"].get<uint64>();

	if (!Json.contains("Primitives"))
	{
		FEngineStatics::NextUUID = SavedNextUUID;
		return true;
	}

	for (auto& [UUIDString, PrimitiveJson] : Json["Primitives"].items())
	{
		uint64 UUID = std::stoull(UUIDString);

		FTransform Transform;

		if (PrimitiveJson.contains("Location"))
		{
			Transform.Location.X = PrimitiveJson["Location"][0].get<float>();
			Transform.Location.Y = PrimitiveJson["Location"][1].get<float>();
			Transform.Location.Z = PrimitiveJson["Location"][2].get<float>();
		}

		if (PrimitiveJson.contains("Rotation"))
		{
			Transform.Rotation.Roll = PrimitiveJson["Rotation"][0].get<float>();
			Transform.Rotation.Pitch = PrimitiveJson["Rotation"][1].get<float>();
			Transform.Rotation.Yaw = PrimitiveJson["Rotation"][2].get<float>();
		}

		if (PrimitiveJson.contains("Scale"))
		{
			Transform.Scale.X = PrimitiveJson["Scale"][0].get<float>();
			Transform.Scale.Y = PrimitiveJson["Scale"][1].get<float>();
			Transform.Scale.Z = PrimitiveJson["Scale"][2].get<float>();
		}

		if (!PrimitiveJson.contains("Type"))
			continue;

		FString TypeString = PrimitiveJson["Type"].get<FString>();

		if (TypeString == "Other")
			continue;

		EPrimitiveType Type = FStringToPrimitiveType(TypeString);

		AStaticMeshActor* Actor =
			World->SpawnActor<AStaticMeshActor>("Test", &Transform);

		if (!Actor)
			continue;

		Actor->SetPrimitiveType(Type);
		Actor->SetUUID(UUID);
	}

	// SpawnActor하면서 증가했을 UUID를 저장 당시 값으로 복원
	FEngineStatics::NextUUID = SavedNextUUID;

	return true;
}
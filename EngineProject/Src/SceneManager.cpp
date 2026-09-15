#include "EnginePCH.h"
#include "SceneManager.h"

#include "Engine/World.h"
#include "Engine/Level.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Actor/StaticMeshActor.h"

#include "Component/PrimitiveComponent.h"

#include "ObjectSystem/Class.h"
#include "ObjectSystem/Casts.h"

bool FSceneManager::Init()
{
	Root.Name = "Root";
	Root.Path = "Root";
	return true;
}

void FSceneManager::Tick(float DeltaTime)
{
}

void FSceneManager::SetWorld(UWorld* InWorld)
{
	World = InWorld;
	CachedLevel = nullptr;
	CachedActors.Reset();
	Root.Clear();
}

void FSceneManager::OnRender()
{
	ImGui::SetNextWindowSize(ImVec2(300.0f, 400.0f), ImGuiCond_FirstUseEver);

	if (!ImGui::Begin("Scene Manager"))
	{
		ImGui::End();
		return;
	}

	if (!World)
	{
		ImGui::TextUnformatted("World : nullptr");
		ImGui::End();
		return;
	}

	ULevel* Level = World->GetCurrentLevel();

	if (!Level)
	{
		ImGui::TextUnformatted("Level : nullptr");
		ImGui::End();
		return;
	}

	if (NeedsRebuild(Level))
		RebuildTree(Level);

	for (FSceneTreeNode* Child : Root.Children)
		DrawTreeNode(Child);

	ImGui::End();
}

bool FSceneManager::NeedsRebuild(ULevel* Level) const
{
	if (!Level)
		return false;

	if (CachedLevel != Level)
		return true;

	const TArray<AActor*>& Actors = Level->GetActors();

	if (Actors.Num() != CachedActors.Num())
		return true;

	for (int32 i = 0; i < Actors.Num(); ++i)
	{
		AActor* Actor = Actors[i];

		if (!Actor)
		{
			if (CachedActors[i].Actor != nullptr)
				return true;

			continue;
		}

		if (CachedActors[i].Actor != Actor)
			return true;

		if (CachedActors[i].UUID != Actor->GetUUID())
			return true;

		if (CachedActors[i].Class != Actor->GetClass())
			return true;

		if (Actor->GetClass() == AStaticMeshActor::StaticClass())
		{
			AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor);

			if (StaticMeshActor && StaticMeshActor->GetPrimitiveComponent())
			{
				int32 CurrentType = static_cast<int32>(StaticMeshActor->GetPrimitiveComponent()->GetType());

				if (CachedActors[i].PrimitiveType != CurrentType)
					return true;
			}
		}
	}

	return false;
}

void FSceneManager::UpdateSnapshot(ULevel* Level)
{
	CachedActors.Reset();
	CachedLevel = Level;

	if (!Level)
		return;

	for (AActor* Actor : Level->GetActors())
	{
		FActorSnapshot Snapshot;
		Snapshot.Actor = Actor;

		if (Actor)
		{
			Snapshot.UUID = Actor->GetUUID();
			Snapshot.Class = Actor->GetClass();

			if (Actor->GetClass() == AStaticMeshActor::StaticClass())
			{
				AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor);

				if (StaticMeshActor && StaticMeshActor->GetPrimitiveComponent())
					Snapshot.PrimitiveType = static_cast<int32>(StaticMeshActor->GetPrimitiveComponent()->GetType());
			}
		}

		CachedActors.Add(Snapshot);
	}
}

void FSceneManager::RebuildTree(ULevel* Level)
{
	Root.Clear();
	Root.Name = "Root";
	Root.Path = "Root";

	if (!Level)
		return;

	for (AActor* Actor : Level->GetActors())
	{
		if (Actor)
			AddActorToTree(&Root, Actor);
	}

	UpdateSnapshot(Level);
}

void FSceneManager::AddActorToTree(FSceneTreeNode* RootNode, AActor* Actor)
{
	if (!RootNode || !Actor)
		return;

	TArray<FString> Path = GetActorTreePath(Actor);

	if (Path.IsEmpty())
		return;

	FSceneTreeNode* CurrentNode = RootNode;

	for (const FString& NodeName : Path)
		CurrentNode = FindOrAddChild(CurrentNode, NodeName);

	CurrentNode->Actors.Add(Actor);
}

TArray<FString> FSceneManager::GetActorTreePath(AActor* Actor) const
{
	TArray<FString> Path;

	if (!Actor)
		return Path;

	UClass* ActorClass = Actor->GetClass();

	if (!ActorClass)
		return Path;

	// 현재 Primitive 구조
	if (ActorClass == AStaticMeshActor::StaticClass())
	{
		AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor);

		if (!StaticMeshActor)
			return Path;

		UPrimitiveComponent* Primitive = StaticMeshActor->GetPrimitiveComponent();

		if (!Primitive)
			return Path;

		Path.Add("Primitive");
		Path.Add(GetPrimitiveTypeName(static_cast<int32>(Primitive->GetType())));
		return Path;
	}

	// 미래에 StaticMeshActor 아래 실제 상속 클래스가 생기는 경우
	if (ActorClass->IsChildOf(AStaticMeshActor::StaticClass()))
	{
		Path.Add("Primitive");

		TArray<UClass*> ClassChain;

		for (UClass* Class = ActorClass; Class && Class != AStaticMeshActor::StaticClass(); Class = Class->Super)
			ClassChain.Add(Class);

		for (auto It = ClassChain.rbegin(); It != ClassChain.rend(); ++It)
			Path.Add(GetClassDisplayName((*It)->Name));

		return Path;
	}

	// 일반 Actor는 AActor 바로 아래부터 표시
	TArray<UClass*> ClassChain;

	for (UClass* Class = ActorClass; Class && Class != AActor::StaticClass(); Class = Class->Super)
		ClassChain.Add(Class);

	for (auto It = ClassChain.rbegin(); It != ClassChain.rend(); ++It)
		Path.Add(GetClassDisplayName((*It)->Name));

	return Path;
}

FSceneManager::FSceneTreeNode* FSceneManager::FindOrAddChild(FSceneTreeNode* Parent, const FString& Name)
{
	if (!Parent)
		return nullptr;

	for (FSceneTreeNode* Child : Parent->Children)
	{
		if (Child && Child->Name == Name)
			return Child;
	}

	FSceneTreeNode* NewNode = new FSceneTreeNode();
	NewNode->Name = Name;
	NewNode->Path = Parent->Path + "/" + Name;

	Parent->Children.Add(NewNode);
	return NewNode;
}

void FSceneManager::DrawTreeNode(FSceneTreeNode* Node)
{
	if (!Node)
		return;

	ImGui::PushID(Node->Path.c_str());

	bool bOpen = ImGui::TreeNodeEx("##SceneTreeNode", ImGuiTreeNodeFlags_DefaultOpen, "%s", Node->Name.c_str());

	if (bOpen)
	{
		for (FSceneTreeNode* Child : Node->Children)
			DrawTreeNode(Child);

		for (AActor* Actor : Node->Actors)
		{
			if (!Actor)
				continue;

			FString ObjectName = Actor->GetName();

			if (ObjectName.empty() || ObjectName == "None")
				ObjectName = Node->Name;

			FString Label = ObjectName + " [UUID: " + std::to_string(Actor->GetUUID()) + "]";

			ImGui::PushID(Actor);

			if (ImGui::Selectable(Label.c_str(), SelectedActor == Actor))
				SelectedActor = Actor;

			ImGui::PopID();
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}

FString FSceneManager::GetPrimitiveTypeName(int32 PrimitiveType) const
{
	switch (static_cast<EPrimitiveType>(PrimitiveType))
	{
	case EPrimitiveType::Cube:	return "Cube";
	case EPrimitiveType::Sphere: return "Sphere";
	case EPrimitiveType::Plane:	return "Plane";
	case EPrimitiveType::Cone:	return "Cone";
	default:					return "Unknown";
	}
}

FString FSceneManager::GetClassDisplayName(const FString& ClassName) const
{
	FString Result = ClassName;

	if (Result.empty())
		return Result;

	if (Result[0] == 'A' || Result[0] == 'U' || Result[0] == 'F')
		Result.erase(Result.begin());

	const FString ActorSuffix = "Actor";

	if (Result.length() >= ActorSuffix.length() &&
		Result.compare(Result.length() - ActorSuffix.length(), ActorSuffix.length(), ActorSuffix) == 0)
	{
		Result.erase(Result.length() - ActorSuffix.length());
	}

	const FString ComponentSuffix = "Component";

	if (Result.length() >= ComponentSuffix.length() &&
		Result.compare(Result.length() - ComponentSuffix.length(), ComponentSuffix.length(), ComponentSuffix) == 0)
	{
		Result.erase(Result.length() - ComponentSuffix.length());
	}

	return Result;
}
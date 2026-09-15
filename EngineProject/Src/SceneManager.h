#pragma once

class UWorld;
class ULevel;
class UClass;
class AActor;

class FSceneManager : public IEditorPanel
{
private:
	struct FSceneTreeNode
	{
		FString Name;
		FString Path;

		TArray<FSceneTreeNode*> Children;
		TArray<AActor*> Actors;

		void Clear()
		{
			for (FSceneTreeNode* Child : Children)
			{
				delete Child;
			}

			Children.Reset();
			Actors.Reset();
		}

		~FSceneTreeNode()
		{
			Clear();
		}
	};

	struct FActorSnapshot
	{
		AActor* Actor = nullptr;
		uint32 UUID = 0;
		UClass* Class = nullptr;
		int32 PrimitiveType = -1;
	};

public:
	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;

	void SetWorld(UWorld* InWorld);

	AActor* GetSelectedActor() const { return SelectedActor; }

private:
	void RebuildTree(ULevel* Level);

	bool NeedsRebuild(ULevel* Level) const;
	void UpdateSnapshot(ULevel* Level);

	void AddActorToTree(FSceneTreeNode* RootNode,AActor* Actor);

	TArray<FString> GetActorTreePath(AActor* Actor) const;

	FSceneTreeNode* FindOrAddChild(FSceneTreeNode* Parent,const FString& Name);

	void DrawTreeNode(FSceneTreeNode* Node);

	FString GetClassDisplayName(const FString& ClassName) const;
	FString GetPrimitiveTypeName(int32 PrimitiveType) const;

private:
	UWorld* World = nullptr;
	ULevel* CachedLevel = nullptr;

	AActor* SelectedActor = nullptr;

	FSceneTreeNode Root;
	TArray<FActorSnapshot> CachedActors;
};
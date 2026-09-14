#pragma once

#include "ObjectSystem/Object.h"
#include "GameFramework/Actor.h"
#include "Component/PrimitiveComponent.h"
#include "Math/Transform.h"
#include "Render/Renderer.h"
#include "PathTracker.h"

class ACameraActor;
class ULevel;


class UWorld : public UObject
{
	DECLARE_CLASS(UWorld, UObject)

public:
	UWorld() = default;
	virtual ~UWorld();

	bool Init();
	/*UPrimitiveComponent* SpawnPrimitive(FClass* Class);*/
	AActor* SpawnActor(UClass* Class, const FTransform* Transform);

	template <class T>
	T* SpawnActor(const FTransform* Transform)
	{
		return CastChecked<T>(SpawnActor(T::StaticClass(), Transform));
	}

	void Tick(float DeltaTime);

	void OnRender(FRenderer* Renderer);

	void ClearScene();
	bool NewScene(const FString& Path);
	bool SaveScene(const FString& Path);
	bool LoadScene(const FString& Path);

	inline void AddPrimitive(UPrimitiveComponent* Primitive) { PrimitiveComponents.Add(Primitive); }
	TArray<UPrimitiveComponent*> GetPrimitiveComponents() const { return PrimitiveComponents; }

	void GatherRenderPackets(TQueue<FRenderPacket>& RenderQueue);
	UPrimitiveComponent* TryPicking(FVector2 MousePos, float ScreenW, float ScreenH);


	///

	void CreateMainCamera();

	// 카메라 Get/Set
	void SetMainCamera(ACameraActor* Camera) { MainCamera = Camera; }
	ACameraActor* GetMainCamera() const { return MainCamera; }
	
	// Level
	ULevel* GetPersistentLevel() const { return PersistentLevel; }
	void SetPersistentLevel(ULevel* InLevel) { PersistentLevel = InLevel; }

	ULevel* GetCurrentLevel() const { return CurrentLevel; }
	void SetCurrentLevel(ULevel* InLevel) { CurrentLevel = InLevel; }

	FPathTracker& GetPathTracker() { return PathTracker; }
	
	int32 GetActorNum();

	bool DestroyActor(AActor* Actor);
	void BeginPlay();
	void EndPlay();

private:
	TQueue<AActor*> BeginPlayList;
	
	TArray<UPrimitiveComponent*> PrimitiveComponents;
		
	//메인 카메라 
	ACameraActor* MainCamera = nullptr;

	FPathTracker PathTracker;

	ULevel* PersistentLevel = nullptr;
	ULevel* CurrentLevel = nullptr;
	TArray<ULevel*> Levels;
};

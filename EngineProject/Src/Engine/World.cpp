#include "EnginePCH.h"
#include "World.h"
#include "Level.h"

#include "ObjectSystem/ObjectFactory.h"
#include "Core/EngineStatics.h"
#include "GameFramework/Actor/StaticMeshActor.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Input/InputSystem.h"

#include "Collision/Ray.h"

UWorld::~UWorld()
{

}

bool  UWorld::Init()
{
	// Spawn Actor로 카메라 생성하고 세팅하기
	PersistentLevel = FObjectFactory::ConstructObject<ULevel>();

	if(!PersistentLevel)
	{
		LOG(Error, "Failed to create PersistentLevel");
		return false;
	}

	//레벨 연결
	PersistentLevel->SetWorld(this);
	Levels.Add(PersistentLevel);
	CurrentLevel = PersistentLevel;

	//카메라 생성
	CreateMainCamera();

	return true;
}

AActor* UWorld::SpawnActor(UClass* Class, FName InName, const FTransform* Transform)
{
	if (!Class) return nullptr;
	if (!Class->IsChildOf(AActor::StaticClass())) return nullptr;

	// 1. ObjectFactory로 Actor 생성
	AActor* NewActor = Cast<AActor>( FObjectFactory::ConstructObject(Class, PersistentLevel, InName) );

	if (!NewActor)
	{
		LOG(Error, "SpawnActor : Failed to create Actor");
		return nullptr;
	}

	// 2. Actor에 World/Level 연결
	NewActor->World = this;
	NewActor->Level = PersistentLevel;

	// 3. Transform 적용
	const FTransform SpawnTransform = Transform ? *Transform : FTransform::Identity;

	if (NewActor->GetRootComponent())
	{
		NewActor->GetRootComponent()->SetTransform(SpawnTransform);
	}

	// 4. Level->Actors에 등록
	PersistentLevel->AddActor(NewActor);

	// 5. PlayList에 추가
	BeginPlayList.Enqueue(NewActor);

	return NewActor;
}

void UWorld::Tick(float DeltaTime)
{
	while (!BeginPlayList.IsEmpty())
	{
		BeginPlayList.Peek()->BeginPlay();
		BeginPlayList.Dequeue();
	}

	for (ULevel* Level : Levels)
	{
		for (AActor* Actor : Level->GetActors())
		{
			Actor->Tick(DeltaTime);
		}
		PathTracker.Tick(Level->GetActors(), DeltaTime);
	}

	if (MainCamera)
	{
		MainCamera->Tick(DeltaTime);
	}

}

void UWorld::OnRender(FRenderer* Renderer)
{
}

void UWorld::ClearScene()
{
	// 렌더링 대상 제거
	PrimitiveComponents.Reset();
	TextComponents.Reset();

	// BeginPlay 대기 중인 Actor 제거
	while (!BeginPlayList.IsEmpty())
	{
		BeginPlayList.Dequeue();
	}

	for (ULevel* Level : Levels)
	{
		// 현재 Scene(Level)의 Actor 제거
		if (CurrentLevel)
		{
			CurrentLevel->ClearActors();
		}
	}
	LOG(Info, "{} : ", PersistentLevel->GetActorNum());
}

void UWorld::GatherRenderPackets(TQueue<FRenderPacket>& RenderQueue)
{
	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (Primitive && Primitive->IsVisible())
			Primitive->SubmitToRenderQueue(RenderQueue);
	}
}

UPrimitiveComponent* UWorld::TryPicking(FVector2 MousePos, float ScreenW, float ScreenH)
{
	FRay ray = MainCamera->GetCameraComponent()->DeProjection(MousePos, ScreenW, ScreenH);

	float minT{ FLT_MAX };
	UPrimitiveComponent* PickingPrimitive = nullptr;		//UPrimitiveComponent*

	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		// ray를 로컬공간으로
		FMatrix invWorld = Primitive->GetWorldMatrix().Inverse();
		FVector4 LocalRayOrigin = invWorld.TransformPosition(ray.Origin);
		FVector4 LocalRayDir = invWorld.TransformVector(ray.Direction);

		FRay LocalRay{};
		LocalRay.Origin.X = LocalRayOrigin.X;
		LocalRay.Origin.Y = LocalRayOrigin.Y;
		LocalRay.Origin.Z = LocalRayOrigin.Z;

		LocalRay.Direction.X = LocalRayDir.X;
		LocalRay.Direction.Y = LocalRayDir.Y;
		LocalRay.Direction.Z = LocalRayDir.Z;

		if (!Primitive) continue;
		const FStaticMeshData& mesh = Primitive->GetMeshData();

		FBox Box = mesh.AABB;
		float rayT{};

		if (!RayIntersectsAABB(LocalRay, Box.Min, Box.Max, rayT))
		{
			continue;
		}

		if (rayT > minT)
		{
			continue;
		}

		// Broad Phase 통과하면 뮐러-트럼보르 알고리즘 수행
		for (uint32 i = 0; i + 2 < mesh.Indices.Num(); i += 3)
		{
			FVector vertices[3]{};	// 3 vertex
			for (uint32 j = 0; j < 3; ++j)
			{
				uint32 index = mesh.Indices[i + j];

				vertices[j].X = mesh.Vertices[index].Position.X;
				vertices[j].Y = mesh.Vertices[index].Position.Y;
				vertices[j].Z = mesh.Vertices[index].Position.Z;
			}

			if (!RayIntersectsTriangle(LocalRay, vertices[0], vertices[1], vertices[2], rayT))
			{
				continue;
			}

			if (rayT < minT)
			{
				PickingPrimitive = Primitive;
				minT = rayT;
			}
		}
	}

	return PickingPrimitive;
}


// 메인 카메라 생성
void UWorld::CreateMainCamera()
{
	if (MainCamera) 
		return;

	MainCamera = FObjectFactory::ConstructObject<ACameraActor>();

	if (!MainCamera) 
	{
		LOG(Error, "Failed to create MainCamera");
		return;
	}

	MainCamera->World = this;
	MainCamera->Level = nullptr;
	MainCamera->GetCameraComponent()->SetRelativeLocation(FVector(-5.0f, -5.0f, 5.0f));
}

int32 UWorld::GetActorNum()
{
	return PersistentLevel->GetActorNum();
}

bool UWorld::DestroyActor(AActor* Actor)
{
	return false;
}

void UWorld::BeginPlay()
{
}

void UWorld::EndPlay()
{
}
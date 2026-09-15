#include "EnginePCH.h"
#include "PathTracker.h"
#include "ObjectSystem/Object.h"

void FPathTracker::SetPathRenderingEnabled(bool bEnable)
{
    bIsPathRenderingEnabled = bEnable;

    if (!bIsPathRenderingEnabled)
    {
        //ObjectPaths.clear();
        TimeSinceLastRecord = 0.0f;
    }
}

void FPathTracker::ClearPath()
{
    ObjectPaths.clear();
}

bool FPathTracker::IsEnabled() const
{
    return bIsPathRenderingEnabled;
}

void FPathTracker::Tick(const TArray<AActor*>& ActorList, float DeltaTime)
{
    if (!bIsPathRenderingEnabled) return;

    TimeSinceLastRecord += DeltaTime;
    if (TimeSinceLastRecord >= RecordInterval)
    {
        TimeSinceLastRecord = 0.0f;

        // 모든 오브젝트를 순회하며 현재 위치를 기록
        for (AActor* Actor : ActorList)
        {
            if (Actor)
            {
                ObjectPaths[Actor].Add(Actor->GetActorLocation());
            }
        }
    }
}

void FPathTracker::OnRender(FLineBatcher* LineBatcher)
{
    if (!bIsPathRenderingEnabled) return;

    for (const auto& Pair : ObjectPaths)
    {
        AActor* Actor = Pair.first;
        const TArray<FVector>& PathHistory = Pair.second;

        if (PathHistory.Num() < 2) continue;

        // 포인터 주소값을 해싱하여 고유한 정수값 생성
        size_t Hash = std::hash<void*>{}(Actor);

        // 비트 연산을 통해 0.0 ~ 1.0 사이의 RGB 값 추출
        float R = ((Hash >> 16) & 0xFF) / 255.0f;
        float G = ((Hash >> 8) & 0xFF) / 255.0f;
        float B = (Hash & 0xFF) / 255.0f;

        // 밝기 보정
        //R = 0.2f + R * 0.8f;
        //G = 0.2f + G * 0.8f;
        //B = 0.2f + B * 0.8f;

        LineBatcher->AddPath(PathHistory, { R, G, B, 1.0f });
    }
}

void FPathTracker::OnObjectDestroyed(AActor* InActor)
{
    ObjectPaths.erase(InActor);
}

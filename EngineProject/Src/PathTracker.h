#pragma once
#include <unordered_map>
#include <vector>

class FPathTracker
{
public:
    void SetPathRenderingEnabled(bool bEnable);

    bool IsEnabled() const;

    void Tick(const TArray<AActor*>& ActorList, float DeltaTime);

    void OnRender(class FLineBatcher* LineBatcher);

    void OnObjectDestroyed(AActor* InActor);

private:
    bool bIsPathRenderingEnabled = true;
    float RecordInterval = 0.01f;
    float TimeSinceLastRecord = 0.0f;

    std::unordered_map<class AActor*, TArray<FVector>> ObjectPaths;
};
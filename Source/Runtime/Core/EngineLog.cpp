#include "EnginePCH.h"
#include "EngineLog.h"

#include <utility>

namespace
{
    TArray<ILogSink*>& GetSinks()
    {
        static TArray<ILogSink*> Sinks;
        return Sinks;
    }

    // Sink가 하나도 없을 때 쌓아 두는 로그 (UE FOutputDeviceRedirector의 backlog)
    // - 엔진 시작 직후(에셋 로딩 등)는 Output Log 패널이 아직 없다
    // - 첫 Sink가 등록되면 순서대로 넘겨주고 비운다
    TArray<std::pair<ELogVerbosity, FString>>& GetBacklog()
    {
        static TArray<std::pair<ELogVerbosity, FString>> Backlog;
        return Backlog;
    }

    // 끝까지 Sink가 없는 빌드(게임)에서 무한히 쌓이지 않게 상한
    constexpr int32 MaxBacklog = 8192;
}

void FLog::AddSink(ILogSink* Sink)
{
    if (!Sink)
        return;

    GetSinks().Add(Sink);

    // 등록 전에 쌓인 로그를 새 Sink에 재생
    TArray<std::pair<ELogVerbosity, FString>>& Backlog = GetBacklog();
    for (const auto& [Verbosity, Message] : Backlog)
        Sink->OnLog(Verbosity, Message);
    Backlog.Empty();
}

void FLog::RemoveSink(ILogSink* Sink)
{
    TArray<ILogSink*>& Sinks = GetSinks();
    for (int32 Index = Sinks.Num() - 1; Index >= 0; --Index)
        if (Sinks[Index] == Sink)
            Sinks.RemoveAt(Index, 1);
}

void FLog::Emit(ELogVerbosity Verbosity, const FString& Message)
{
    TArray<ILogSink*>& Sinks = GetSinks();

    // Sink 없음 → 보관 (상한을 넘으면 버림)
    if (Sinks.IsEmpty())
    {
        TArray<std::pair<ELogVerbosity, FString>>& Backlog = GetBacklog();
        if (Backlog.Num() < MaxBacklog)
            Backlog.Add({ Verbosity, Message });
        return;
    }

    for (ILogSink* Sink : Sinks)
        Sink->OnLog(Verbosity, Message);
}

#include "EnginePCH.h"
#include "ObjectFactory.h"

UObject* FObjectFactory::ConstructObject(UClass* Class)
{
    if (!Class || !Class->Constructor)
        return nullptr;
    
    UObject* Result = Class->Constructor();

    LOG(Info, "Create {}", Class->Name);
    LOG(Info, "Total Allocation Bytes - {}", FEngineStatics::TotalAllocationBytes);
    LOG(Info, "Total Allocation Count - {}", FEngineStatics::TotalAllocationCount);

    return Result;
}

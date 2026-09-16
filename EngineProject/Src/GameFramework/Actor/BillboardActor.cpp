#include "EnginePCH.h"
#include "BillboardActor.h"

ABillboardActor::ABillboardActor()
{
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>();
	SetRootComponent(BillboardComponent);
}

ABillboardActor::~ABillboardActor()
{
}


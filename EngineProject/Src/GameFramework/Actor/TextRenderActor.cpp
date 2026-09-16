#include "EnginePCH.h"
#include "TextRenderActor.h"

ATextRenderActor::ATextRenderActor()
{
	TextRenderComponent = CreateDefaultSubobject<UTextRenderComponent>();
	SetRootComponent(TextRenderComponent);
}

ATextRenderActor::~ATextRenderActor()
{
}

#include "EnginePCH.h"
#include "EditorUI.h"

bool FEditorUI::Init()
{
	return true;
}

void FEditorUI::Tick(float DeltaTime)
{
	for (auto& Panel : Panels)
	{
		Panel->Tick(DeltaTime);
	}
}

void FEditorUI::OnRender()
{
	for (auto& Panel : Panels)
	{
		Panel->OnRender();
	}
}

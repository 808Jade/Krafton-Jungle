#pragma once

#include "EditorPanel.h"

class AssetBrowserPanel : public IEditorPanel
{
public:
	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;

private:

	
};
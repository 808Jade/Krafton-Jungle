#pragma once

#include "EditorPanel.h"

#include <filesystem>

namespace fs = std::filesystem;

class FAssetBrowserPanel : public IEditorPanel
{
public:
	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;

	void RenderFolderTree(const fs::path& Path, fs::path& SelectedPath);

private:
	fs::path AssetRootPath = "Assets/";
	fs::path CurrentPath;
	
};
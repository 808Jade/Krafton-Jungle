#pragma once

#include <format>
#include "EditorPanel.h"

struct FEditorSettings
{
	// Toggles
	bool bWireframe = false;
	bool bDrawPrimitives = true;
	bool bDrawBoundingBox = false;
	bool bShowUUID = false;
	bool bDrawBatchLine = true;
	bool bDrawPSGrid = false;

	// Values
	float CameraSpeed = 1.0f;
	float MouseSensitivity = 1.0f;
	int32 GridSpacing = 1;
};


class FConfigPanel : public IEditorPanel
{
public:
	FConfigPanel() = default;
	~FConfigPanel();

	bool Init() override;
	void Tick(float DeltaTime)override;
	void OnRender() override;

	void SetWorld(UWorld* InWorld) { World = InWorld; }

	const FEditorSettings& GetSettings() const { return Settings; }
	bool SaveSettings() const;
	bool LoadSettings();

private:
	UWorld* World;
	FEditorSettings Settings;
};


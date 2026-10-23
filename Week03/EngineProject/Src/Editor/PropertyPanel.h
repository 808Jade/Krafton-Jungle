#pragma once

#include <format>
#include "EditorPanel.h"

struct FTransform;

class FPropertyPanel : public IEditorPanel
{
public:
	FPropertyPanel() = default;
	~FPropertyPanel();

	bool Init() override;
	void Tick(float DeltaTime)override;
	void OnRender() override;

	void SetTarget(USceneComponent* InTargetOrNull) { Target = InTargetOrNull; }

	void SetWorld(UWorld* InWorld) { World = InWorld; }

	ImFont* GetCustomFont() { return CustomFont; }

private:
	UWorld* World;
	USceneComponent* Target;

	ImFont* CustomFont;
};


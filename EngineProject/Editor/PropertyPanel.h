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
	
	void SetTarget(USceneComponent* InTarget) { Target = InTarget; }

	UWorld* World;

	USceneComponent* Target;
};


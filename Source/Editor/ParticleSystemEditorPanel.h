#pragma once
#include "EditorUI/EditorPanel.h"


class FParticleSystemEditorPanel : public IEditorPanel
{
public:
	FParticleSystemEditorPanel() = default;
	~FParticleSystemEditorPanel() = default;
	FParticleSystemEditorPanel(const FParticleSystemEditorPanel&) = delete;
	FParticleSystemEditorPanel& operator=(const FParticleSystemEditorPanel&) = delete;

	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;
	void DrawToolBar();

private:
	TArray<TUniquePtr<IEditorPanel>> Panels;




};
#pragma once
#include "Editor/EditorUI/EditorPanel.h"

class FParticleEditorContext;

// 파티클 에디터 ⑤ 디테일 패널. Context의 선택에 따라 파티클 시스템 또는 선택한 이미터의 프로퍼티를 보여 준다.
class FParticleSystemDetailsPanel : public IEditorPanel
{
public:
	explicit FParticleSystemDetailsPanel(FParticleEditorContext& InContext) : Context(InContext) {}

	bool Init() override { return true; }
	void Tick(float DeltaTime) override {}
	void OnRender() override;
	// ##Particle: 레벨 에디터의 "Details" 창과 구분한다.
	const char* GetPanelName() const override { return "Details##Particle"; }

private:
	void DrawParticleSystemProperties();
	void DrawEmitterProperties();

	// 참조(집약): 호스트가 소유하는 공유 상태
	FParticleEditorContext& Context;
};

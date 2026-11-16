#pragma once
#include "Editor/EditorUI/EditorPanel.h"

class FParticleEditorContext;

// 파티클 에디터 ④ 이미터 패널. 이미터를 열(column)로 나열하고 선택을 Context에 기록한다.
// 지금은 에셋 모델(UParticleSystem·UParticleEmitter·모듈)이 없어 런타임 이미터 인스턴스를 보여 준다.
class FParticleSystemEmitterPanel : public IEditorPanel
{
public:
	explicit FParticleSystemEmitterPanel(FParticleEditorContext& InContext) : Context(InContext) {}

	bool Init() override { return true; }
	void Tick(float DeltaTime) override {}
	void OnRender() override;
	const char* GetPanelName() const override { return "Emitters##Particle"; }

private:
	void DrawEmitterColumn(int32 EmitterIndex);

	// 참조(집약): 호스트가 소유하는 공유 상태
	FParticleEditorContext& Context;
};

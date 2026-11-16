#pragma once
#include "Editor/EditorUI/EditorPanel.h"

class FParticleEditorContext;
class UParticleEmitter;
class UParticleModule;

// 파티클 에디터 ④ 이미터 패널. Template의 이미터를 열(column)로 나열하고, 각 열에 모듈 스택을 보여 준다.
// 이미터·모듈 선택은 Context에 기록한다. 디테일 패널이 그 선택을 읽어 그린다.
class FParticleSystemEmitterPanel : public IEditorPanel
{
public:
	explicit FParticleSystemEmitterPanel(FParticleEditorContext& InContext) : Context(InContext) {}

	bool Init() override { return true; }
	void Tick(float DeltaTime) override {}
	void OnRender() override;
	const char* GetPanelName() const override { return "Emitters##Particle"; }

private:
	void DrawEmitterColumn(int32 EmitterIndex, UParticleEmitter* Emitter);
	// 모듈 한 줄. 클릭하면 이 이미터와 모듈을 함께 선택한다.
	void DrawModuleRow(int32 EmitterIndex, UParticleModule* Module);

	// 참조(집약): 호스트가 소유하는 공유 상태
	FParticleEditorContext& Context;
};

#pragma once
#include "Editor/EditorUI/EditorPanel.h"

class FParticleEditorContext;
class UParticleModule;
struct FProperty;

// 파티클 에디터 ⑤ 디테일 패널. Context의 선택에 따라 모듈, 이미터, 파티클 시스템 순으로 우선해서 보여 준다.
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
	// 선택한 모듈의 리플렉션 프로퍼티(PROPERTY로 등록한 값)를 기반 클래스부터 그린다.
	void DrawModuleProperties(UParticleModule* Module);
	// 프로퍼티 한 줄. 값을 바꿔 적용했으면 true다.
	bool DrawModuleProperty(UParticleModule* Module, const FProperty& Property);

	// 참조(집약): 호스트가 소유하는 공유 상태
	FParticleEditorContext& Context;
};

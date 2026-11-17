#pragma once
#include "Editor/EditorUI/EditorPanel.h"

class FParticleEditorContext;
class UParticleEmitter;
class UParticleModule;
class UClass;

// 파티클 에디터 ④ 이미터 패널. Template의 이미터를 열(column)로 나열하고, 각 열에 모듈 스택을 보여 준다.
// 이미터·모듈 선택은 Context에 기록한다. 디테일 패널이 그 선택을 읽어 그린다.
// 우클릭 메뉴로 모듈 추가·삭제와 TypeData 추가·제거를 한다. (Cascade의 이미터 우클릭 메뉴와 같은 역할)
class FParticleSystemEmitterPanel : public IEditorPanel
{
public:
	// InTabId: 소속 탭의 ID. 같은 에디터 탭이 여러 개 열려도 창 ID가 겹치지 않게 이름 뒤에 붙인다.
	FParticleSystemEmitterPanel(FParticleEditorContext& InContext, const FString& InTabId)
		: PanelName("Emitters###Emitters_" + InTabId), Context(InContext) {}

	bool Init() override { return true; }
	void Tick(float DeltaTime) override {}
	void OnRender() override;
	const char* GetPanelName() const override { return PanelName.c_str(); }

private:
	void DrawEmitterColumn(int32 EmitterIndex, UParticleEmitter* Emitter);
	// 모듈 한 줄. 클릭하면 이 이미터와 모듈을 함께 선택한다.
	void DrawModuleRow(int32 EmitterIndex, UParticleModule* Module);
	// 우클릭 메뉴 본문. ClickedModule이 있으면 맨 위에 그 모듈 삭제 항목을 둔다.
	void DrawContextMenu(int32 EmitterIndex, UParticleModule* ClickedModule);
	// 이미터 추가 항목. 이미터 열 메뉴와 패널 빈 곳 메뉴가 함께 쓴다.
	void DrawNewEmitterMenuItem();
	// 그리는 동안 모듈 배열을 바꾸면 순회가 깨지므로, 메뉴에서 고른 편집은 모아 두었다가 다 그린 뒤 적용한다.
	void ApplyPendingEdit();

	enum class EPendingEdit : uint8
	{
		None,
		AddModule,
		DeleteModule,
		AddMeshTypeData,
		AddEmitter,
		DeleteEmitter,
	};
	struct FPendingEdit
	{
		EPendingEdit Type = EPendingEdit::None;
		int32 EmitterIndex = -1;
		UClass* ModuleClass = nullptr;	// AddModule
		UParticleModule* Module = nullptr;	// DeleteModule
	};
	// 소유(값): 한 프레임 동안만 유지되는 편집 요청
	FPendingEdit PendingEdit;

	FString PanelName;

	// 참조(집약): 호스트가 소유하는 공유 상태
	FParticleEditorContext& Context;
};

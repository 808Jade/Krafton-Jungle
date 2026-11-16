#include "EnginePCH.h"
#include "Editor/ParticleSystem/ParticleSystemEmitterPanel.h"
#include "Editor/ParticleSystem/ParticleEditorContext.h"

#include "Components/ParticleSystemComponent.h"

namespace
{
	constexpr float EmitterColumnWidth = 200.0f;
}

void FParticleSystemEmitterPanel::OnRender()
{
	bool bOpen = IsOpen();
	if (ImGui::Begin(GetPanelName(), &bOpen, ImGuiWindowFlags_HorizontalScrollbar))
	{
		const UParticleSystemComponent* Component = Context.GetPreviewComponent();
		const int32 EmitterCount = Component ? Component->EmitterInstances.Num() : 0;

		if (EmitterCount == 0)
		{
			ImGui::TextDisabled("No emitters.");
		}
		for (int32 EmitterIndex = 0; EmitterIndex < EmitterCount; ++EmitterIndex)
		{
			if (EmitterIndex > 0)
				ImGui::SameLine();
			DrawEmitterColumn(EmitterIndex);
		}

		// 빈 영역을 클릭하면 선택을 해제한다. 디테일 패널은 파티클 시스템 전체를 보여 준다.
		if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			Context.SelectedEmitterIndex = INDEX_NONE;
	}
	ImGui::End();

	SetOpen(bOpen);
}

void FParticleSystemEmitterPanel::DrawEmitterColumn(const int32 EmitterIndex)
{
	const FParticleEmitterInstance* Instance = Context.GetPreviewComponent()->EmitterInstances[EmitterIndex];
	const bool bSelected = Context.SelectedEmitterIndex == EmitterIndex;

	ImGui::PushID(EmitterIndex);
	ImGui::BeginChild("##EmitterColumn", ImVec2(EmitterColumnWidth, 0.0f), ImGuiChildFlags_Borders);

	// 열 머리: 클릭하면 이 이미터를 선택한다. (Cascade의 이미터 머리와 같은 역할)
	const FString Header = "Emitter " + std::to_string(EmitterIndex);
	if (ImGui::Selectable(Header.c_str(), bSelected, ImGuiSelectableFlags_None, ImVec2(0.0f, 28.0f)))
		Context.SelectedEmitterIndex = EmitterIndex;

	if (Instance)
		ImGui::Text("Active %d / %d", Instance->ActiveParticles, Instance->MaxActiveParticles);
	ImGui::Separator();

	// 모듈 스택 자리. 에셋 모델이 생기면 Required·Spawn·Lifetime… 모듈을 여기에 나열한다.
	ImGui::TextDisabled("Modules: no asset");

	ImGui::EndChild();
	ImGui::PopID();
}

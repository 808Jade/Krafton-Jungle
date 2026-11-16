#include "EnginePCH.h"
#include "Editor/ParticleSystem/ParticleSystemEmitterPanel.h"
#include "Editor/ParticleSystem/ParticleEditorContext.h"

#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/Modules/ParticleModuleRequired.h"
#include "Particles/Modules/ParticleModuleTypeDataBase.h"

#include <algorithm>

namespace
{
	constexpr float EmitterColumnWidth = 200.0f;
	constexpr float EmitterHeaderHeight = 28.0f;

	// 클래스 이름에서 공통 접두사를 떼어 Cascade처럼 짧게 보여 준다. (UParticleModuleSpawn → Spawn)
	FString GetModuleDisplayName(const UParticleModule* Module)
	{
		static const FString Prefix = "UParticleModule";
		FString Name = Module->GetClass()->GetName();
		if (Name.rfind(Prefix, 0) == 0 && Name.size() > Prefix.size())
			Name.erase(0, Prefix.size());
		return Name;
	}

	// 툴바의 LOD 번호에 해당하는 LOD 레벨. 그 번호가 없으면 마지막 레벨을 쓴다.
	UParticleLODLevel* GetDisplayedLODLevel(const UParticleEmitter* Emitter, const int32 LODIndex)
	{
		if (!Emitter || Emitter->LODLevels.IsEmpty())
			return nullptr;
		const int32 Clamped = std::clamp(LODIndex, 0, Emitter->LODLevels.Num() - 1);
		return Emitter->LODLevels[Clamped];
	}
}

void FParticleSystemEmitterPanel::OnRender()
{
	bool bOpen = IsOpen();
	if (ImGui::Begin(GetPanelName(), &bOpen, ImGuiWindowFlags_HorizontalScrollbar))
	{
		const UParticleSystem* Template = Context.GetTemplate();
		if (!Template)
		{
			ImGui::TextDisabled("No particle system asset.");
		}
		else if (Template->Emitters.IsEmpty())
		{
			ImGui::TextDisabled("No emitters.");
		}
		else
		{
			for (int32 EmitterIndex = 0; EmitterIndex < Template->Emitters.Num(); ++EmitterIndex)
			{
				if (EmitterIndex > 0)
					ImGui::SameLine();
				DrawEmitterColumn(EmitterIndex, Template->Emitters[EmitterIndex]);
			}
		}

		// 빈 영역을 클릭하면 선택을 해제한다. 디테일 패널은 파티클 시스템 전체를 보여 준다.
		if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			Context.SelectedEmitterIndex = INDEX_NONE;
			Context.SelectedModule = nullptr;
		}
	}
	ImGui::End();

	SetOpen(bOpen);
}

void FParticleSystemEmitterPanel::DrawEmitterColumn(const int32 EmitterIndex, UParticleEmitter* Emitter)
{
	ImGui::PushID(EmitterIndex);
	// 열 안의 빈 곳 클릭은 열 자신의 hover로 처리되므로, 선택 해제는 열 바깥 빈 곳에서만 일어난다.
	ImGui::BeginChild("##EmitterColumn", ImVec2(EmitterColumnWidth, 0.0f), ImGuiChildFlags_Borders);

	// 열 머리: 클릭하면 이미터 자체를 선택한다. (Cascade의 이미터 머리와 같은 역할)
	const bool bEmitterSelected = Context.SelectedEmitterIndex == EmitterIndex && !Context.SelectedModule;
	const FString Header = "Emitter " + std::to_string(EmitterIndex);
	if (ImGui::Selectable(Header.c_str(), bEmitterSelected, ImGuiSelectableFlags_None, ImVec2(0.0f, EmitterHeaderHeight)))
	{
		Context.SelectedEmitterIndex = EmitterIndex;
		Context.SelectedModule = nullptr;
	}

	if (!Emitter)
	{
		ImGui::TextDisabled("(null emitter)");
		ImGui::EndChild();
		ImGui::PopID();
		return;
	}

	UParticleLODLevel* LODLevel = GetDisplayedLODLevel(Emitter, Context.CurrentLODIndex);
	const bool bMeshEmitter = LODLevel && LODLevel->TypeDataModule;
	ImGui::TextDisabled("%s", bMeshEmitter ? "Mesh" : "Sprite");

	// 활성 파티클 수는 런타임 인스턴스에서 읽는다. InitParticles가 Template->Emitters 순서대로 인스턴스를 만든다.
	const UParticleSystemComponent* Component = Context.GetPreviewComponent();
	if (Component && EmitterIndex < Component->EmitterInstances.Num() && Component->EmitterInstances[EmitterIndex])
	{
		const FParticleEmitterInstance* Instance = Component->EmitterInstances[EmitterIndex];
		ImGui::Text("Active %d / %d", Instance->ActiveParticles, Instance->MaxActiveParticles);
	}
	else
	{
		ImGui::TextDisabled("Not simulating");
	}
	ImGui::Separator();

	if (!LODLevel)
	{
		ImGui::TextDisabled("No LOD levels.");
	}
	else
	{
		// Cascade와 같은 순서: TypeData → Required → 나머지 모듈(배열 순서).
		if (LODLevel->TypeDataModule)
			DrawModuleRow(EmitterIndex, LODLevel->TypeDataModule);
		if (LODLevel->RequiredModule)
			DrawModuleRow(EmitterIndex, LODLevel->RequiredModule);
		for (UParticleModule* Module : LODLevel->Modules)
		{
			if (Module)
				DrawModuleRow(EmitterIndex, Module);
		}
	}

	ImGui::EndChild();
	ImGui::PopID();
}

void FParticleSystemEmitterPanel::DrawModuleRow(const int32 EmitterIndex, UParticleModule* Module)
{
	ImGui::PushID(Module);

	const bool bSelected = Context.SelectedModule == Module;
	// 꺼진 모듈은 흐리게 보여 준다. 선택은 그대로 가능하다.
	if (!Module->bEnabled)
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

	const FString Name = GetModuleDisplayName(Module);
	if (ImGui::Selectable(Name.c_str(), bSelected))
	{
		Context.SelectedEmitterIndex = EmitterIndex;
		Context.SelectedModule = Module;
	}

	if (!Module->bEnabled)
		ImGui::PopStyleColor();

	ImGui::PopID();
}

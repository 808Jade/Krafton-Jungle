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
#include "Particles/Modules/ParticleModuleLifetime.h"
#include "Particles/Modules/ParticleModuleLocation.h"
#include "Particles/Modules/ParticleModuleVelocity.h"
#include "Particles/Modules/ParticleModuleSize.h"
#include "Particles/Modules/ParticleModuleColor.h"
#include "Particles/Modules/ParticleModuleRotation.h"
#include "Particles/Modules/ParticleModuleRotationRate.h"
#include "Particles/Modules/ParticleModuleAccelerationConstant.h"
#include "Particles/Modules/ParticleModuleCollision.h"
#include "Particles/Modules/ParticleModuleAttractorPoint.h"
#include "Particles/Modules/ParticleModuleSizeScaleBySpeed.h"
#include "Particles/Modules/ParticleModuleSubUV.h"
#include "Particles/Modules/ParticleModuleSubUVMovie.h"

#include <algorithm>

namespace
{
	constexpr float EmitterColumnWidth = 200.0f;
	constexpr float EmitterHeaderHeight = 28.0f;

	// 우클릭 메뉴의 모듈 추가 항목. Cascade처럼 분류 하위 메뉴 아래에 표시 이름으로 나열한다.
	// Required·Spawn은 이미터마다 하나씩 고정이라 추가 대상이 아니다. 새 모듈 클래스가 생기면 여기에 한 줄 추가한다.
	struct FAddableModule
	{
		const char* Category;
		const char* DisplayName;
		UClass* (*GetClass)();
	};
	const FAddableModule AddableModules[] = {
		{ "Acceleration",  "Const Acceleration",    [] { return UParticleModuleAccelerationConstant::StaticClass(); } },
		{ "Attraction",    "Point Attractor",       [] { return UParticleModuleAttractorPoint::StaticClass(); } },
		{ "Collision",     "Collision",             [] { return UParticleModuleCollision::StaticClass(); } },
		{ "Color",         "Initial Color",         [] { return UParticleModuleColor::StaticClass(); } },
		{ "Lifetime",      "Lifetime",              [] { return UParticleModuleLifetime::StaticClass(); } },
		{ "Location",      "Initial Location",      [] { return UParticleModuleLocation::StaticClass(); } },
		{ "Rotation",      "Initial Rotation",      [] { return UParticleModuleRotation::StaticClass(); } },
		{ "Rotation Rate", "Initial Rotation Rate", [] { return UParticleModuleRotationRate::StaticClass(); } },
		{ "Size",          "Initial Size",          [] { return UParticleModuleSize::StaticClass(); } },
		{ "Size",          "Size By Speed",         [] { return UParticleModuleSizeScaleBySpeed::StaticClass(); } },
		{ "SubUV",         "SubImage Index",        [] { return UParticleModuleSubUV::StaticClass(); } },
		{ "SubUV",         "SubUV Movie",           [] { return UParticleModuleSubUVMovie::StaticClass(); } },
		{ "Velocity",      "Initial Velocity",      [] { return UParticleModuleVelocity::StaticClass(); } },
	};

	// 모듈 클래스 이름 → Cascade 표시 이름. 언리얼은 클래스 메타데이터(DisplayName)로 정하는 값이다.
	// 클래스 이름은 언리얼과 같으므로 표시 이름도 언리얼 것을 그대로 쓴다.
	struct FModuleDisplayName
	{
		const char* ClassName;
		const char* DisplayName;
	};
	constexpr FModuleDisplayName ModuleDisplayNames[] = {
		{ "UParticleModuleRequired",      "Required" },
		{ "UParticleModuleSpawn",         "Spawn" },
		{ "UParticleModuleLifetime",      "Lifetime" },
		{ "UParticleModuleLocation",      "Initial Location" },
		{ "UParticleModuleVelocity",      "Initial Velocity" },
		{ "UParticleModuleSize",          "Initial Size" },
		{ "UParticleModuleColor",         "Initial Color" },
		{ "UParticleModuleRotation",      "Initial Rotation" },
		{ "UParticleModuleRotationRate",  "Initial Rotation Rate" },
		{ "UParticleModuleTypeDataMesh",  "Mesh Data" },
		{ "UParticleModuleAccelerationConstant", "Const Acceleration" },
		{ "UParticleModuleAttractorPoint", "Point Attractor" },
		{ "UParticleModuleSizeScaleBySpeed", "Size By Speed" },
		{ "UParticleModuleSubUV", "SubImage Index" },
		{ "UParticleModuleSubUVMovie", "SubUV Movie" },
	};

	// 표에 없는 새 모듈은 클래스 이름에서 공통 접두사만 떼어 보여 준다. (UParticleModuleDrag → Drag)
	FString GetModuleDisplayName(const UParticleModule* Module)
	{
		const FString ClassName = Module->GetClass()->GetName();
		for (const FModuleDisplayName& Entry : ModuleDisplayNames)
		{
			if (ClassName == Entry.ClassName)
				return Entry.DisplayName;
		}

		static const FString Prefix = "UParticleModule";
		FString Name = ClassName;
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

	ApplyPendingEdit();
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
	if (ImGui::BeginPopupContextItem("##EmitterHeaderMenu"))
	{
		DrawContextMenu(EmitterIndex, nullptr);
		ImGui::EndPopup();
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

	// 열의 빈 곳을 우클릭해도 같은 메뉴를 연다. 항목 위 우클릭은 각 항목의 메뉴가 처리한다.
	if (ImGui::BeginPopupContextWindow("##EmitterColumnMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		DrawContextMenu(EmitterIndex, nullptr);
		ImGui::EndPopup();
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
	// 우클릭도 언리얼처럼 그 모듈을 먼저 선택한다.
	if (ImGui::Selectable(Name.c_str(), bSelected) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
	{
		Context.SelectedEmitterIndex = EmitterIndex;
		Context.SelectedModule = Module;
	}

	if (!Module->bEnabled)
		ImGui::PopStyleColor();

	// 표시 이름은 언리얼 것이지만 실제 클래스를 확인할 수 있게 툴팁에 남긴다.
	if (ImGui::IsItemHovered() && !ImGui::IsPopupOpen("##ModuleMenu"))
		ImGui::SetTooltip("%s", Module->GetClass()->GetName().c_str());

	if (ImGui::BeginPopupContextItem("##ModuleMenu"))
	{
		DrawContextMenu(EmitterIndex, Module);
		ImGui::EndPopup();
	}

	ImGui::PopID();
}

void FParticleSystemEmitterPanel::DrawContextMenu(const int32 EmitterIndex, UParticleModule* ClickedModule)
{
	const UParticleLODLevel* LODLevel = Context.GetLODLevel(EmitterIndex);
	if (!LODLevel)
	{
		ImGui::TextDisabled("No LOD level to edit.");
		return;
	}

	if (ClickedModule)
	{
		const bool bCanDelete = FParticleEditorContext::CanDeleteModule(ClickedModule);
		if (ImGui::MenuItem("Delete Module", nullptr, false, bCanDelete))
			PendingEdit = { EPendingEdit::DeleteModule, EmitterIndex, nullptr, ClickedModule };
		if (!bCanDelete && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Required and Spawn modules cannot be deleted.");
		ImGui::Separator();
	}

	// TypeData는 이미터당 하나다. 있으면 새로 만들 수 없고, 지우면 Sprite 이미터로 돌아간다.
	if (ImGui::BeginMenu("TypeData"))
	{
		if (ImGui::MenuItem("New Mesh Data", nullptr, false, LODLevel->TypeDataModule == nullptr))
			PendingEdit = { EPendingEdit::AddMeshTypeData, EmitterIndex, nullptr, nullptr };
		if (ImGui::MenuItem("Remove TypeData", nullptr, false, LODLevel->TypeDataModule != nullptr))
			PendingEdit = { EPendingEdit::DeleteModule, EmitterIndex, nullptr, LODLevel->TypeDataModule };
		ImGui::EndMenu();
	}
	ImGui::Separator();

	// 분류 하위 메뉴. 표는 분류 순서로 정렬되어 있어 같은 분류끼리 붙어 있다.
	// 같은 종류의 모듈이 이미 있으면 비활성으로 보여 준다.
	for (int32 Index = 0; Index < static_cast<int32>(std::size(AddableModules)); ++Index)
	{
		const FAddableModule& Entry = AddableModules[Index];
		if (Index > 0 && strcmp(AddableModules[Index - 1].Category, Entry.Category) == 0)
			continue;	// 이 분류는 앞에서 이미 열었다.

		if (!ImGui::BeginMenu(Entry.Category))
			continue;
		for (int32 Inner = Index; Inner < static_cast<int32>(std::size(AddableModules))
			&& strcmp(AddableModules[Inner].Category, Entry.Category) == 0; ++Inner)
		{
			UClass* ModuleClass = AddableModules[Inner].GetClass();
			const bool bAlreadyAdded = Context.HasModuleOfClass(EmitterIndex, ModuleClass);
			if (ImGui::MenuItem(AddableModules[Inner].DisplayName, bAlreadyAdded ? "added" : nullptr, false, !bAlreadyAdded))
				PendingEdit = { EPendingEdit::AddModule, EmitterIndex, ModuleClass, nullptr };
		}
		ImGui::EndMenu();
	}
}

void FParticleSystemEmitterPanel::ApplyPendingEdit()
{
	const FPendingEdit Edit = PendingEdit;
	PendingEdit = {};

	switch (Edit.Type)
	{
	case EPendingEdit::AddModule:
		Context.AddModule(Edit.EmitterIndex, Edit.ModuleClass);
		break;
	case EPendingEdit::DeleteModule:
		Context.DeleteModule(Edit.EmitterIndex, Edit.Module);
		break;
	case EPendingEdit::AddMeshTypeData:
		Context.AddMeshTypeData(Edit.EmitterIndex);
		break;
	default:
		break;
	}
}

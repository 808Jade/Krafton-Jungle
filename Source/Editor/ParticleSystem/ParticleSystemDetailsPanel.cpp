#include "EnginePCH.h"
#include "Editor/ParticleSystem/ParticleSystemDetailsPanel.h"
#include "Editor/ParticleSystem/ParticleEditorContext.h"

#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleRange.h"
#include "Render/MaterialInterface.h"
#include "Render/Mesh.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"

#include <cstring>

namespace
{
	// Struct 프로퍼티는 리플렉션에 타입 이름이 없어 크기로 구분한다. 세 범위 타입의 크기가 서로 달라야 한다.
	static_assert(sizeof(FParticleFloatRange) != sizeof(FParticleVectorRange)
		&& sizeof(FParticleFloatRange) != sizeof(FParticleColorRange)
		&& sizeof(FParticleVectorRange) != sizeof(FParticleColorRange),
		"Particle range structs must differ in size to be told apart in the details panel.");

	// Enum 프로퍼티도 리플렉션에 값 이름이 없다. 프로퍼티 이름으로 표시 이름 표를 찾는다.
	// 순서는 ParticleTypes.h의 enum 선언 순서와 같아야 한다.
	struct FEnumDisplay
	{
		const char* PropertyName;
		const char* Items;	// ImGui::Combo 형식 ('\0'으로 구분, 끝은 "\0\0")
	};
	constexpr FEnumDisplay EnumDisplays[] = {
		{ "SortMode",        "None\0Distance To View\0View Depth\0" },
		{ "ScreenAlignment", "Facing Camera Position\0Square\0Rectangle\0Velocity\0" },
		{ "LockAxis",        "None\0X\0Y\0Z\0" },
	};

	const char* FindEnumItems(const FString& PropertyName)
	{
		for (const FEnumDisplay& Entry : EnumDisplays)
		{
			if (PropertyName == Entry.PropertyName)
				return Entry.Items;
		}
		return nullptr;
	}

	// 표시용 모듈 이름. 클래스 이름에서 공통 접두사만 뗀다. (UParticleModuleSize → Size)
	FString GetModuleTitle(const UParticleModule* Module)
	{
		static const FString Prefix = "UParticleModule";
		FString Name = Module->GetClass()->GetName();
		if (Name.rfind(Prefix, 0) == 0 && Name.size() > Prefix.size())
			Name.erase(0, Prefix.size());
		return Name;
	}

	FString GetAssetLabel(const URenderAsset* Asset)
	{
		if (!Asset)
			return "None";
		const FString& Path = Asset->GetPath();
		const size_t Slash = Path.find_last_of("/\\");
		return Slash == FString::npos ? Path : Path.substr(Slash + 1);
	}

	// Mesh·Material처럼 에셋을 가리키는 프로퍼티의 선택 콤보. 고른 에셋을 OutPicked에 넣고 true를 돌려준다.
	template <typename TAsset>
	bool DrawAssetCombo(const char* Id, TAsset* Current, TAsset*& OutPicked)
	{
		bool bPicked = false;
		if (ImGui::BeginCombo(Id, GetAssetLabel(Current).c_str()))
		{
			if (ImGui::Selectable("None", Current == nullptr))
			{
				OutPicked = nullptr;
				bPicked = true;
			}
			for (TObjectIterator<TAsset> It; It; ++It)
			{
				TAsset* Asset = *It;
				ImGui::PushID(Asset);
				if (ImGui::Selectable(GetAssetLabel(Asset).c_str(), Asset == Current))
				{
					OutPicked = Asset;
					bPicked = true;
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", Asset->GetPath().c_str());
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}
		return bPicked;
	}
}

void FParticleSystemDetailsPanel::OnRender()
{
	bool bOpen = IsOpen();
	if (ImGui::Begin(GetPanelName(), &bOpen))
	{
		// 모듈 > 이미터 > 파티클 시스템 순으로 가장 좁은 선택을 보여 준다. (Cascade와 같은 규칙)
		if (Context.SelectedModule)
			DrawModuleProperties(Context.SelectedModule);
		else if (Context.GetSelectedEmitterInstance())
			DrawEmitterProperties();
		else
			DrawParticleSystemProperties();
	}
	ImGui::End();

	SetOpen(bOpen);
}

void FParticleSystemDetailsPanel::DrawParticleSystemProperties()
{
	UParticleSystemComponent* Component = Context.GetPreviewComponent();
	if (!Component)
	{
		ImGui::TextDisabled("No particle system.");
		return;
	}

	UParticleSystem* Template = Context.GetTemplate();
	if (Template && ImGui::CollapsingHeader("Particle System", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool bBoundsChanged = ImGui::Checkbox("Use Fixed Bounds", &Template->bUseFixedRelativeBoundingBox);
		ImGui::BeginDisabled(!Template->bUseFixedRelativeBoundingBox);
		bBoundsChanged |= ImGui::DragFloat3("Fixed Bounds Min", &Template->FixedRelativeBoundingBox.Min.X, 0.05f);
		bBoundsChanged |= ImGui::DragFloat3("Fixed Bounds Max", &Template->FixedRelativeBoundingBox.Max.X, 0.05f);
		ImGui::EndDisabled();
		if (bBoundsChanged)
		{
			Component->OnTransformDirty();
			Context.MarkDirty();
		}
	}

	if (ImGui::CollapsingHeader("Simulation", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Paused", &Context.bSimulationPaused);
		ImGui::Text("Emitters: %d", Component->EmitterInstances.Num());
	}
}

void FParticleSystemDetailsPanel::DrawEmitterProperties()
{
	FParticleEmitterInstance* Instance = Context.GetSelectedEmitterInstance();

	if (ImGui::CollapsingHeader("Emitter", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Index: %d", Context.SelectedEmitterIndex);
		ImGui::Text("Active Particles: %d / %d", Instance->ActiveParticles, Instance->MaxActiveParticles);
		ImGui::Checkbox("Use Local Space", &Instance->bUseLocalSpace);
	}

	if (ImGui::CollapsingHeader("Bounds", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const FBox& Box = Instance->GetBoundingBox();
		ImGui::Text("Min: %.2f, %.2f, %.2f", Box.Min.X, Box.Min.Y, Box.Min.Z);
		ImGui::Text("Max: %.2f, %.2f, %.2f", Box.Max.X, Box.Max.Y, Box.Max.Z);
	}

	ImGui::Spacing();
	ImGui::TextDisabled("Select a module in the Emitters panel to edit its properties.");
}

void FParticleSystemDetailsPanel::DrawModuleProperties(UParticleModule* Module)
{
	ImGui::Text("%s", GetModuleTitle(Module).c_str());
	ImGui::TextDisabled("Emitter %d", Context.SelectedEmitterIndex);
	ImGui::Separator();

	// 기반 클래스(UParticleModule의 bEnabled)부터 파생 클래스 순으로 그린다.
	TArray<UClass*> ClassChain;
	for (UClass* Class = Module->GetClass(); Class; Class = Class->Super)
		ClassChain.Add(Class);

	bool bCommitted = false;
	int32 DrawnCount = 0;
	ImGui::PushID(Module);
	for (auto It = ClassChain.rbegin(); It != ClassChain.rend(); ++It)
	{
		for (const FProperty& Property : (*It)->GetProperties())
		{
			if (!Property.HasAnyFlags(CPF_Edit) || !Module->IsPropertyVisibleInDetails(Property))
				continue;

			const bool bReadOnly = Property.HasAnyFlags(CPF_EditConst);
			if (bReadOnly) ImGui::BeginDisabled();
			bCommitted |= DrawModuleProperty(Module, Property);
			if (bReadOnly) ImGui::EndDisabled();
			++DrawnCount;
		}
	}
	ImGui::PopID();

	if (DrawnCount == 0)
		ImGui::TextDisabled("This module has no editable properties.");

	// 드래그 중에는 값만 바꾸고, 편집을 마친 순간 한 번만 캐시를 다시 만들고 다시 시작한다.
	// (매 프레임 다시 시작하면 드래그하는 동안 파티클이 계속 지워져 결과를 볼 수 없다.)
	if (bCommitted)
		Context.NotifyModuleEdited(Context.SelectedEmitterIndex);
}

bool FParticleSystemDetailsPanel::DrawModuleProperty(UParticleModule* Module, const FProperty& Property)
{
	const FString Name = Property.Name.ToString();
	void* ValuePtr = Property.ContainerPtrToValuePtr(Module);
	bool bCommitted = false;
	// 마지막으로 그린 위젯의 편집이 끝났는지(마우스를 놓거나 Enter) 확인한다.
	auto CheckCommit = [&bCommitted]() { bCommitted |= ImGui::IsItemDeactivatedAfterEdit(); };

	ImGui::PushID(Name.c_str());
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(Name.c_str());
	ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.42f);
	ImGui::SetNextItemWidth(-1.0f);

	switch (Property.Type)
	{
	case EPropertyType::Float:
	{
		float Value = *static_cast<const float*>(ValuePtr);
		if (ImGui::DragFloat("##Value", &Value, 0.05f))
			Module->ApplyPropertyValue(Property.Name, &Value);
		CheckCommit();
		break;
	}
	case EPropertyType::Int:
	{
		int Value = *static_cast<const int*>(ValuePtr);
		if (ImGui::DragInt("##Value", &Value, 1.0f))
			Module->ApplyPropertyValue(Property.Name, &Value);
		CheckCommit();
		break;
	}
	case EPropertyType::Bool:
	{
		bool Value = *static_cast<const bool*>(ValuePtr);
		if (ImGui::Checkbox("##Value", &Value))
		{
			Module->ApplyPropertyValue(Property.Name, &Value);
			bCommitted = true;
		}
		break;
	}
	case EPropertyType::Vector:
	{
		FVector Value = *static_cast<const FVector*>(ValuePtr);
		if (ImGui::DragFloat3("##Value", Value.V, 0.05f))
			Module->ApplyPropertyValue(Property.Name, &Value);
		CheckCommit();
		break;
	}
	case EPropertyType::LinearColor:
	{
		FLinearColor Value = *static_cast<const FLinearColor*>(ValuePtr);
		if (ImGui::ColorEdit4("##Value", Value.RGBA, ImGuiColorEditFlags_Float))
			Module->ApplyPropertyValue(Property.Name, &Value);
		CheckCommit();
		break;
	}
	case EPropertyType::Enum:
	{
		// 기반 정수형 크기만큼만 읽고 쓴다. (파티클 enum은 uint8이지만 다른 크기도 안전하게 처리한다)
		int64 Raw = 0;
		std::memcpy(&Raw, ValuePtr, Property.Size);
		int Current = static_cast<int>(Raw);
		bool bChanged = false;
		if (const char* Items = FindEnumItems(Name))
			bChanged = ImGui::Combo("##Value", &Current, Items);
		else
			bChanged = ImGui::InputInt("##Value", &Current);	// 표에 없는 enum은 숫자로 편집한다.

		if (bChanged)
		{
			int64 NewRaw = Current;
			alignas(int64) uint8 Buffer[sizeof(int64)] = {};
			std::memcpy(Buffer, &NewRaw, Property.Size);
			Module->ApplyPropertyValue(Property.Name, Buffer);
			bCommitted = true;
		}
		break;
	}
	case EPropertyType::Struct:
	{
		// 범위(Min~Max) 구조체. 두 줄로 Min과 Max를 따로 편집한다.
		ImGui::NewLine();
		ImGui::Indent();
		if (Property.Size == sizeof(FParticleFloatRange))
		{
			FParticleFloatRange Value = *static_cast<const FParticleFloatRange*>(ValuePtr);
			bool bChanged = ImGui::DragFloat("Min", &Value.Min, 0.05f);
			CheckCommit();
			bChanged |= ImGui::DragFloat("Max", &Value.Max, 0.05f);
			CheckCommit();
			if (bChanged)
				Module->ApplyPropertyValue(Property.Name, &Value);
		}
		else if (Property.Size == sizeof(FParticleVectorRange))
		{
			FParticleVectorRange Value = *static_cast<const FParticleVectorRange*>(ValuePtr);
			bool bChanged = ImGui::DragFloat3("Min", Value.Min.V, 0.05f);
			CheckCommit();
			bChanged |= ImGui::DragFloat3("Max", Value.Max.V, 0.05f);
			CheckCommit();
			if (bChanged)
				Module->ApplyPropertyValue(Property.Name, &Value);
		}
		else if (Property.Size == sizeof(FParticleColorRange))
		{
			FParticleColorRange Value = *static_cast<const FParticleColorRange*>(ValuePtr);
			bool bChanged = ImGui::ColorEdit4("Min", Value.Min.RGBA, ImGuiColorEditFlags_Float);
			CheckCommit();
			bChanged |= ImGui::ColorEdit4("Max", Value.Max.RGBA, ImGuiColorEditFlags_Float);
			CheckCommit();
			if (bChanged)
				Module->ApplyPropertyValue(Property.Name, &Value);
		}
		else
		{
			ImGui::TextDisabled("(Unsupported struct)");
		}
		ImGui::Unindent();
		break;
	}
	case EPropertyType::Object:
	{
		UObject* Current = Property.GetObjectPropertyValue(ValuePtr);
		if (Property.Class && Property.Class->IsChildOf(UStaticMesh::StaticClass()))
		{
			UStaticMesh* Picked = nullptr;
			if (DrawAssetCombo<UStaticMesh>("##Value", Cast<UStaticMesh>(Current), Picked))
				bCommitted = Module->ApplyObjectPropertyValue(Property.Name, Picked);
		}
		else if (Property.Class && Property.Class->IsChildOf(UMaterialInterface::StaticClass()))
		{
			UMaterialInterface* Picked = nullptr;
			if (DrawAssetCombo<UMaterialInterface>("##Value", Cast<UMaterialInterface>(Current), Picked))
				bCommitted = Module->ApplyObjectPropertyValue(Property.Name, Picked);
		}
		else
		{
			ImGui::TextDisabled("%s", Current ? Current->GetName().c_str() : "None");
		}
		break;
	}
	default:
		ImGui::TextDisabled("(Unsupported)");
		break;
	}

	ImGui::PopID();
	return bCommitted;
}

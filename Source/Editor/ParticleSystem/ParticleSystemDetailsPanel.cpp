#include "EnginePCH.h"
#include "Editor/ParticleSystem/ParticleSystemDetailsPanel.h"
#include "Editor/ParticleSystem/ParticleEditorContext.h"

#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleRange.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/Modules/ParticleModuleRequired.h"
#include "Particles/Modules/ParticleModuleSpawn.h"
#include "Particles/Modules/ParticleModuleTypeDataRibbon.h"
#include "Render/MaterialInterface.h"
#include "Render/Mesh.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"

#include <cstring>
#include <cmath>

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
		{ "Method", "Distribution\0User Set\0Actor\0Component\0" },
		{ "DistributionType", "Constant\0Uniform\0Constant Curve\0" },
		{ "SortMode",        "None\0Distance To View\0View Depth\0" },
		{ "ScreenAlignment", "Facing Camera Position\0Square\0Rectangle\0Velocity\0" },
		{ "LockAxis",        "None\0X\0Y\0Z\0" },
		{ "CollisionCompletionOption", "Kill\0Freeze\0Halt Collisions\0" },
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
				// 경로가 없는 객체(런타임에 만든 머티리얼 인스턴스 등)는 이름이 비어 빈 칸으로 보이고,
				// 에셋 파일에도 저장할 수 없으므로 목록에서 뺀다.
				if (Asset->GetPath().empty())
					continue;
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

	if (Template && ImGui::CollapsingHeader("LOD", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool bLODChanged = false;

		static const char* LODMethodNames[] = { "Automatic", "DirectSet", "ActivateAutomatic" };
		int32 LODMethodIndex = static_cast<int32>(Template->LODMethod);
		if (ImGui::Combo("LOD Method", &LODMethodIndex, LODMethodNames, IM_ARRAYSIZE(LODMethodNames)))
		{
			Template->LODMethod = static_cast<EParticleSystemLODMethod>(LODMethodIndex);
			bLODChanged = true;
		}

		if (ImGui::DragFloat("Distance Check Time", &Template->LODDistanceCheckTime, 0.01f, 0.01f, 10.0f, "%.2f s"))
		{
			Template->LODDistanceCheckTime = std::max(Template->LODDistanceCheckTime, 0.01f);
			bLODChanged = true;
		}

		TArray<float>& Distances = Template->LODDistances;
		for (int32 Index = 0; Index < Distances.Num(); ++Index)
		{
			ImGui::PushID(Index);
			const FString Label = "LOD " + std::to_string(Index) + " Distance";
			if (Index == 0)
			{
				ImGui::BeginDisabled();
				float Zero = 0.0f;
				ImGui::DragFloat(Label.c_str(), &Zero);
				ImGui::EndDisabled();
			}
			else
			{
				const float MinDistance = Distances[Index - 1];
				const float MaxDistance = Index + 1 < Distances.Num() ? Distances[Index + 1] : FLT_MAX;
				if (ImGui::DragFloat(Label.c_str(), &Distances[Index], 0.5f, MinDistance, MaxDistance, "%.1f"))
				{
					Distances[Index] = std::clamp(Distances[Index], MinDistance, MaxDistance);
					bLODChanged = true;
				}
			}
			ImGui::PopID();
		}

		if (bLODChanged)
			Context.MarkDirty();
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
		UParticleLODLevel* LODLevel =
		    Context.GetLODLevel(Context.SelectedEmitterIndex);

		if (LODLevel && LODLevel->RequiredModule)
		{
			const bool bRibbon =
			    Cast<UParticleModuleTypeDataRibbon>(
			        LODLevel->TypeDataModule) != nullptr;

			bool bLocalSpace =
			    LODLevel->RequiredModule->bUseLocalSpace;

			ImGui::BeginDisabled(bRibbon);

			const bool bChanged =
			    ImGui::Checkbox("Use Local Space", &bLocalSpace);

			ImGui::EndDisabled();

			if (bRibbon)
			{
				ImGui::TextDisabled(
				    "Ribbon currently supports world space only.");
			}

			if (bChanged)
			{
				LODLevel->RequiredModule->bUseLocalSpace = bLocalSpace;
				Context.NotifyModuleEdited(Context.SelectedEmitterIndex);
				return;
			}
		}

		if (LODLevel)
		{
			const FString Label = "Enabled in LOD " + std::to_string(Context.CurrentLODIndex);
			if (ImGui::Checkbox(Label.c_str(), &LODLevel->bEnabled))
				Context.NotifyModuleEdited(Context.SelectedEmitterIndex);
		}
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

			const UParticleLODLevel* LODLevel =
			    Context.GetLODLevel(Context.SelectedEmitterIndex);

			const bool bRibbonLocalSpace =
			    LODLevel &&
			    Cast<UParticleModuleTypeDataRibbon>(
			        LODLevel->TypeDataModule) &&
			    Cast<UParticleModuleRequired>(Module) &&
			    Property.Name.ToString() == "bUseLocalSpace";

			const bool bReadOnly =
			    Property.HasAnyFlags(CPF_EditConst) ||
			    bRibbonLocalSpace;

			if (bReadOnly) ImGui::BeginDisabled();
			bCommitted |= DrawModuleProperty(Module, Property);
			if (bReadOnly) ImGui::EndDisabled();
			++DrawnCount;
		}
	}
	if (auto* Spawn = Cast<UParticleModuleSpawn>(Module))
	{
		bCommitted |= DrawSpawnBursts(Spawn);
		++DrawnCount;
	}
	ImGui::PopID();

	if (DrawnCount == 0)
		ImGui::TextDisabled("This module has no editable properties.");

	// 드래그 중에는 값만 바꾸고, 편집을 마친 순간 한 번만 캐시를 다시 만들고 다시 시작한다.
	// (매 프레임 다시 시작하면 드래그하는 동안 파티클이 계속 지워져 결과를 볼 수 없다.)
	if (bCommitted)
		Context.NotifyModuleEdited(Context.SelectedEmitterIndex);
}

bool FParticleSystemDetailsPanel::DrawSpawnBursts(UParticleModuleSpawn* Spawn)
{
	bool bCommitted = false;
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextUnformatted("Bursts");
	ImGui::TextDisabled("Time is in seconds from the start of each emitter loop.");
	if (Spawn->Bursts.IsEmpty())
		ImGui::TextDisabled("No bursts. Add one to spawn a group of particles at once.");

	int32 RemoveIndex = INDEX_NONE;
	if (!Spawn->Bursts.IsEmpty() && ImGui::BeginTable("##Bursts", 3,
	    ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("Time (seconds)");
		ImGui::TableSetupColumn("Count");
		ImGui::TableSetupColumn("##Actions", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Remove").x + ImGui::GetStyle().FramePadding.x * 2.0f);
		ImGui::TableHeadersRow();
		for (int32 Index = 0; Index < Spawn->Bursts.Num(); ++Index)
		{
			FParticleBurst& Burst = Spawn->Bursts[Index];
			ImGui::PushID(Index);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::SetNextItemWidth(-1.0f);
			float Time = Burst.Time;
			if (ImGui::InputFloat("##Time", &Time, 0.0f, 0.0f, "%.3f") && std::isfinite(Time))
				Burst.Time = std::max(Time, 0.0f);
			bCommitted |= ImGui::IsItemDeactivatedAfterEdit();

			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-1.0f);
			int Count = Burst.Count;
			if (ImGui::InputInt("##Count", &Count)) Burst.Count = std::max(Count, 0);
			bCommitted |= ImGui::IsItemDeactivatedAfterEdit();

			ImGui::TableSetColumnIndex(2);
			if (ImGui::SmallButton("Remove")) RemoveIndex = Index;
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	// Defer removal until no widgets retain references into the array.
	if (RemoveIndex != INDEX_NONE)
	{
		Spawn->Bursts.RemoveAt(RemoveIndex);
		bCommitted = true;
	}
	if (ImGui::Button("Add Burst"))
	{
		Spawn->Bursts.Add({ 0.0f, 1 });
		bCommitted = true;
	}
	return bCommitted;
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
	case EPropertyType::Name:
	{
		FString Value = static_cast<const FName*>(ValuePtr)->ToString();
		char Buffer[256] = {};
		strncpy_s(Buffer, Value.c_str(), _TRUNCATE);
		if (ImGui::InputText("##Value", Buffer, sizeof(Buffer)))
		{
			const FName NewValue(Buffer);
			Module->ApplyPropertyValue(Property.Name, &NewValue);
		}
		CheckCommit();
		break;
	}
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
	case EPropertyType::String:
	{
		// 문자열은 고정 길이 버퍼로 편집한다. (이벤트 이름처럼 짧은 값용)
		char Buffer[128];
		const FString& Current = *static_cast<const FString*>(ValuePtr);
		std::snprintf(Buffer, sizeof(Buffer), "%s", Current.c_str());
		if (ImGui::InputText("##Value", Buffer, sizeof(Buffer)))
		{
			FString NewValue = Buffer;
			Module->ApplyPropertyValue(Property.Name, &NewValue);
		}
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

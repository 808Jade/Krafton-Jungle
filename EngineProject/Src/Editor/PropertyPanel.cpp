#include "EnginePCH.h"
#include "PropertyPanel.h"

#include "imgui_internal.h"
#include "EditorDragDrop.h"
#include "Component/PrimitiveComponent.h"
#include "Render/Material.h"
#include "Render/Texture2D.h"
#include "Text/Font.h"

namespace
{
	bool DrawAxisControl(const FString& _label, float& _value, float _speed, float _minValue, float _maxValue, float _resetValue, FVector4 _color)
	{
		bool isValueChanged = false;

		float lineHeight = GImGui->Font->LegacySize + GImGui->Style.FramePadding.y * 2.0f;
		ImVec4 color(_color.X, _color.Y, _color.Z, _color.W);
		ImVec2 buttonSize = { lineHeight + 2.0f , lineHeight };
		// 각 버튼의 색상 설정
		ImGui::PushStyleColor(ImGuiCol_Button, color);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ color.x + 0.1f, color.y + 0.1f, color.z + 0.1f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ color.x - 0.1f, color.y - 0.1f, color.z - 0.1f, 1.0f });

		// 굵은 폰트 적용
		if (ImGui::Button(_label.c_str(), buttonSize))
		{
			_value = _resetValue;
			isValueChanged = true;
		}
		ImGui::PopStyleColor(3);

		// 드래그 슬라이더
		ImGui::SameLine();
		std::string dragID = "##" + _label;
		isValueChanged |= ImGui::DragFloat(dragID.c_str(), &_value, _speed, _minValue, _maxValue, "%.2f");

		return isValueChanged;

	}

	bool DrawVector3Controller(const FString& _label, FVector& _values, float _resetValue, float _columnWidth)
	{
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;

		bool isVectorChanged = false;

		ImGui::PushID(_label.c_str());
		if (ImGui::BeginTable(_label.c_str(), 2, flags)) // 고유 ID, 열 2개, 플래그
		{
			// ImGuiTableColumnFlags_WidthFixed: 초기 너비 고정
			// ImGuiTableColumnFlags_WidthStretch: 창 크기에 따라 너비 조절 (기본값)
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, _columnWidth);
			ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(_label.c_str()); // 왼쪽 열: 레이블

			ImGui::TableSetColumnIndex(1);
			// [컨트롤러 UI 코드] // 오른쪽 열: 컨트롤러
			ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 2,0 });
			ImGui::PushFont(boldFont);

			isVectorChanged |= DrawAxisControl("X", _values.X, 0.1f, 0.0f, 0.0f, 0.0f, { 0.8f, 0.1f,0.1f, 1.0f });

			ImGui::PopItemWidth();

			ImGui::SameLine();
			isVectorChanged |= DrawAxisControl("Y", _values.Y, 0.1f, 0.0f, 0.0f, 0.0f, { 0.1f, 0.8f,0.1f, 1.0f });

			ImGui::PopItemWidth();

			ImGui::SameLine();
			isVectorChanged |= DrawAxisControl("Z", _values.Z, 0.1f, 0.0f, 0.0f, 0.0f, { 0.1f, 0.1f,0.8f, 1.0f });

			ImGui::PopItemWidth();

			ImGui::PopFont();
			ImGui::PopStyleVar();
			ImGui::EndTable();
		}

		ImGui::Columns(1);

		ImGui::PopID();

		return isVectorChanged;
	}

	void DrawTextureSlot(UMaterial** MaterialPtr, int32 SlotIndex)
	{
		const float ThumbnailSize = 64.0f;

		UMaterial* Material = *MaterialPtr;
		UTexture2D* Texture = nullptr;
		if (SlotIndex < static_cast<int32>(Material->Textures.Num()))
		{
			Texture = Material->Textures[SlotIndex];
		}

		ImGui::PushID(SlotIndex);

		if (Texture && Texture->GetResource())
		{
			ImGui::Image(
				(ImTextureID)Texture->GetResource()->GetSRV(),
				{ ThumbnailSize, ThumbnailSize });
		}
		else
		{
			ImGui::Button("Empty", { ThumbnailSize, ThumbnailSize });
		}

		// 바로 위 위젯이 드롭을 받는다
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(EditorDragDrop::Texture))
			{
				UTexture2D* DroppedTexture = *static_cast<UTexture2D**>(Payload->Data);

				// 공유 에셋이면 지금 복제한다.
				// 이 시점까지 미루면 손대지 않은 오브젝트는 계속 공유 에셋을 쓴다.
				if (Material->bIsInstance == false)
				{
					Material = UMaterial::CreateInstance(Material);
					*MaterialPtr = Material;
				}

				// 슬롯이 배열 범위 밖이면 그 자리까지 늘려 준다
				while (static_cast<int32>(Material->Textures.Num()) <= SlotIndex)
				{
					Material->Textures.Add(nullptr);
				}

				Material->Textures[SlotIndex] = DroppedTexture;
				Texture = DroppedTexture;
			}

			ImGui::EndDragDropTarget();
		}

		ImGui::SameLine();

		ImGui::BeginGroup();
		ImGui::Text("Slot %d", SlotIndex);

		if (Texture)
		{
			ImGui::TextWrapped("%s", Texture->GetPath().c_str());
		}
		else
		{
			ImGui::TextDisabled("(비어 있음)");
		}
		ImGui::EndGroup();

		ImGui::PopID();
	}

	void DrawFontSlot(UFont** FontPtr)
	{
		const float ThumbnailSize = 64.0f;

		UFont* Font = *FontPtr;

		if (Font && Font->AtlasTexture && Font->AtlasTexture->GetResource())
		{
			ImGui::Image(
				(ImTextureID)Font->AtlasTexture->GetResource()->GetSRV(),
				{ ThumbnailSize, ThumbnailSize });
		}
		else
		{
			ImGui::Button("Empty", { ThumbnailSize, ThumbnailSize });
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(EditorDragDrop::Font))
			{
				*FontPtr = *static_cast<UFont**>(Payload->Data);
				Font = *FontPtr;
			}

			ImGui::EndDragDropTarget();
		}

		ImGui::SameLine();

		ImGui::BeginGroup();
		if (Font)
		{
			ImGui::TextWrapped("%s", Font->GetPath().c_str());
		}
		else
		{
			ImGui::TextDisabled("(비어 있음)");
		}
		ImGui::EndGroup();
	}


	// UClass에 등록된 프로퍼티를 타입에 맞는 위젯으로 그린다
	void DrawProperty(UObject* Object, const FProperty& Property, ImFont* CustomFont)
	{
		void* ValuePtr = reinterpret_cast<char*>(Object) + Property.Offset;
		const FString Label = "##" + Property.Name;

		ImGui::Text(Property.Name.c_str());
		ImGui::SameLine(120.0f);
		ImGui::SetNextItemWidth(-1.0f);

		switch (Property.Type)
		{
		case EPropertyType::Float:
			ImGui::DragFloat(Label.c_str(), static_cast<float*>(ValuePtr), 0.1f);
			break;

		case EPropertyType::Int:
			ImGui::DragInt(Label.c_str(), static_cast<int*>(ValuePtr), 1.0f);
			break;

		case EPropertyType::Bool:
			ImGui::Checkbox(Label.c_str(), static_cast<bool*>(ValuePtr));
			break;

		case EPropertyType::Vector:
		{
			FVector* Value = static_cast<FVector*>(ValuePtr);
			ImGui::DragFloat3(Label.c_str(), &Value->X, 0.1f);
			break;
		}

		case EPropertyType::Vector4:
		{
			FVector4* Value = static_cast<FVector4*>(ValuePtr);
			ImGui::DragFloat4(Label.c_str(), &Value->X, 0.1f);
			break;
		}

		case EPropertyType::Color:
		{
			// 타입은 Vector4와 같고 위젯만 색상 선택기다
			FVector4* Value = static_cast<FVector4*>(ValuePtr);
			ImGui::ColorEdit4(Label.c_str(), &Value->X);
			break;
		}

		case EPropertyType::String:
		{
			FString* Value = static_cast<FString*>(ValuePtr);

			char Buffer[256] = {};
			strncpy_s(Buffer, Value->c_str(), sizeof(Buffer) - 1);
			if (CustomFont) ImGui::PushFont(CustomFont);
			if (ImGui::InputText(Label.c_str(), Buffer, sizeof(Buffer)))
			{
				*Value = Buffer;
			}
			if (CustomFont) ImGui::PopFont();
			break;
		}

		case EPropertyType::Material:
		{
			// ValuePtr은 UMaterial* 자체의 주소다.
			// 드롭이 일어나면 DrawTextureSlot이 여기에 복제본 포인터를 써넣는다.
			UMaterial** MaterialPtr = static_cast<UMaterial**>(ValuePtr);
			if (*MaterialPtr == nullptr)
			{
				ImGui::TextDisabled("(머티리얼 없음)");
				break;
			}

			// Textures 배열을 순회하지 않고 슬롯 0을 고정으로 그린다.
			// 배열이 비어 있어도 드롭할 자리가 있어야 하기 때문.
			DrawTextureSlot(MaterialPtr, 0);
			break;
		}

		case EPropertyType::Font:
		{
			DrawFontSlot(static_cast<UFont**>(ValuePtr));
			break;
		}

		default:
			ImGui::TextDisabled("(Unsupported)");
			break;
		}
	}
	// 클래스 계층을 따라 올라가며 각 단계의 프로퍼티를 표시
	void DrawProperties(UObject* Object, ImFont* CustomFont)
	{
		if (!Object)
		{
			return;
		}

		TArray<UClass*> ClassChain;
		for (UClass* Class = Object->GetClass(); Class; Class = Class->Super)
		{
			ClassChain.Add(Class);
		}

		// 기반 클래스부터 표시
		for (auto It = ClassChain.rbegin(); It != ClassChain.rend(); ++It)
		{
			UClass* Class = *It;
			if (Class->GetProperties().IsEmpty())
			{
				continue;
			}

			ImGui::PushID(Class->Name.c_str());
			if (ImGui::CollapsingHeader(Class->Name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (const FProperty& Property : Class->GetProperties())
				{
					DrawProperty(Object, Property, CustomFont);
				}
			}
			ImGui::PopID();
		}
	}
}

bool FPropertyPanel::Init()
{
	ImGuiIO& io = ImGui::GetIO();

	io.Fonts->AddFontDefault();

	const char* fontPath = "../HMKMRHD.ttf";
	float fontSize = 15.0f;
	const ImWchar* koreanRanges = io.Fonts->GetGlyphRangesKorean();

	CustomFont = io.Fonts->AddFontFromFileTTF(fontPath, fontSize, nullptr, koreanRanges);
	assert(CustomFont != nullptr);

	return true;
}

void FPropertyPanel::Tick(float DeltaTime)
{
}


void FPropertyPanel::OnRender()
{
	ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);

	ImGui::Begin("Jungle Property Window");

	if (Target)
	{
		FTransform Transform = Target->GetTransform();
		DrawVector3Controller("Location", Transform.Location, 0.0f, 10.0f);

		FVector Rotation = FVector(Transform.Rotation.Roll, Transform.Rotation.Pitch, Transform.Rotation.Yaw);
		DrawVector3Controller("Rotation", Rotation, 0.0f, 10.0f);

		Transform.Rotation = FRotator(Rotation.Y, Rotation.Z, Rotation.X);
		DrawVector3Controller("Scale", Transform.Scale, 0.0f, 10.0f);

		Target->SetTransform(Transform);

		ImGui::Separator();

		// 액터 -> 컴포넌트 순으로, 클래스별 프로퍼티 표시
		DrawProperties(Target->GetOwner(), CustomFont);
		// 선택된 컴포넌트뿐 아니라 같은 액터의 다른 컴포넌트도 보여준다.
		// (예: 라이트는 빌보드를 클릭해서 고르지만 수치는 SpotLight 쪽에 있다)
		if (AActor* Owner = Target->GetOwner())
		{
			for (UActorComponent* Component : Owner->GetComponents())
			{
				// 같은 클래스를 상속한 컴포넌트가 여럿이면 헤더 ID가 겹치므로 분리한다
				ImGui::PushID(Component);
				DrawProperties(Component, CustomFont);
				ImGui::PopID();
			}
		}
		else
		{
			DrawProperties(Target, CustomFont);
		}
	}

	ImGui::End();
}

FPropertyPanel::~FPropertyPanel()
{
	//delete transform;
}
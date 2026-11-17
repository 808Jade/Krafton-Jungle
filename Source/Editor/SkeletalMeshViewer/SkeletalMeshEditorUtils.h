#pragma once

#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifyEvent.h"
#include "Animation/AnimDataModel.h"
#include "Math/Vector.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <string>

// 스켈레탈 메시 에디터의 Context와 여러 패널이 함께 쓰는 작은 도우미. (예전 SkeletalPreviewContext.cpp 안의 익명 이름공간)
namespace SkeletalMeshEditorUtils
{
	inline float SRGBToLinear(float C)
	{
		C = std::clamp(C, 0.0f, 1.0f);
		return C <= 0.04045f ? C / 12.92f : std::pow((C + 0.055f) / 1.055f, 2.4f);
	}

	inline FVector SRGBToLinear(const FVector& C)
	{
		return FVector(SRGBToLinear(C.X), SRGBToLinear(C.Y), SRGBToLinear(C.Z));
	}

	inline FAnimNotifyEvent* FindPreviewNotify(UAnimSequence* Sequence, int32 Index)
	{
		if (!Sequence || Index < 0 || Index >= Sequence->Notifies.Num())
		{
			return nullptr;
		}
		return &Sequence->Notifies[Index];
	}

	inline FString GetNotifyLabel(const FAnimNotifyEvent& Notify)
	{
		return Notify.Notify ? Notify.Notify->GetNotifyName() : Notify.NotifyName.ToString();
	}

	inline bool IsNotifyStateEvent(const FAnimNotifyEvent& Notify)
	{
		return Notify.IsStateEvent();
	}

	inline bool IsNotifyStateClass(const UClass* Class)
	{
		return Class && Class->IsChildOf(UAnimNotifyState::StaticClass());
	}

	inline bool CanHaveNotifyDuration(const FAnimNotifyEvent& Notify)
	{
		return Notify.Notify == nullptr || IsNotifyStateClass(Notify.Notify->GetClass());
	}

	inline float SnapNotifyTimeToFrame(const UAnimSequence& Sequence, float Time)
	{
		const UAnimDataModel* Model = Sequence.GetDataModel();
		const float FPS = Model ? Model->GetFrameRate().AsDecimal() : 0.0f;
		if (FPS <= 0.0f)
		{
			return Time;
		}
		return std::round(Time * FPS) / FPS;
	}
	constexpr const char* PreviewLockedTooltip = "Animation preview: select None in the Animation window to edit";

	inline void ShowEditTooltip(bool bLocked, const char* NormalTooltip = nullptr)
	{
		if (bLocked)
		{
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			{
				ImGui::SetTooltip("%s", PreviewLockedTooltip);
			}
		}
		else if (NormalTooltip && ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", NormalTooltip);
		}
	}

	enum class ETransportIcon : uint8
	{
		Play,
		Pause,
		Reverse,
		ToFirst,
		ToLast,
		StepBack,
		StepForward,
		Loop,
	};

	inline bool IsPlayableClip(const UAnimSequence* Sequence)
	{
		const UAnimDataModel* Model = Sequence ? Sequence->GetDataModel() : nullptr;
		return Model && Model->GetNumberOfFrames() > 0 && Model->GetPlayLength() > 0.0f;
	}

	inline std::string FormatPlaySpeed(float Speed)
	{
		char Buffer[32];
		snprintf(Buffer, sizeof(Buffer), "%.2f", Speed);
		std::string Text(Buffer);
		while (Text.size() > 3 && Text.back() == '0' && Text[Text.size() - 2] != '.')
		{
			Text.pop_back();
		}
		return "x" + Text;
	}

	inline bool DrawTransportButton(const char* Id, ETransportIcon Icon, bool bActive)
	{
		const float Size = ImGui::GetFrameHeight();
		const ImVec2 Min = ImGui::GetCursorScreenPos();
		const ImVec2 Max(Min.x + Size, Min.y + Size);

		const bool bPressed = ImGui::InvisibleButton(Id, ImVec2(Size, Size));
		const bool bHovered = ImGui::IsItemHovered();
		const bool bHeld = ImGui::IsItemActive();

		ImDrawList* DrawList = ImGui::GetWindowDrawList();

		ImGuiCol BgCol = ImGuiCol_Button;
		if (bHeld) BgCol = ImGuiCol_ButtonActive;
		else if (bHovered) BgCol = ImGuiCol_ButtonHovered;
		DrawList->AddRectFilled(Min, Max, ImGui::GetColorU32(BgCol), ImGui::GetStyle().FrameRounding);

		const ImU32 IconCol = ImGui::GetColorU32(bActive ? ImGuiCol_CheckMark : ImGuiCol_Text);
		const float Cx = (Min.x + Max.x) * 0.5f;
		const float Cy = (Min.y + Max.y) * 0.5f;
		const float R = Size * 0.28f;
		const float Bar = R * 0.35f;

		auto Triangle = [&](float TipX, float Dir, float Width)
		{
			const float BaseX = TipX - Dir * Width;
			DrawList->AddTriangleFilled(ImVec2(BaseX, Cy - R), ImVec2(TipX, Cy), ImVec2(BaseX, Cy + R), IconCol);
		};
		auto VerticalBar = [&](float X0)
		{
			DrawList->AddRectFilled(ImVec2(X0, Cy - R), ImVec2(X0 + Bar, Cy + R), IconCol);
		};

		switch (Icon)
		{
		case ETransportIcon::Play:
			Triangle(Cx + R, +1.0f, R * 1.8f);
			break;
		case ETransportIcon::Reverse:
			Triangle(Cx - R, -1.0f, R * 1.8f);
			break;
		case ETransportIcon::Pause:
			VerticalBar(Cx - R * 0.7f);
			VerticalBar(Cx + R * 0.7f - Bar);
			break;
		case ETransportIcon::ToFirst:
			VerticalBar(Cx - R);
			Triangle(Cx - R + Bar, -1.0f, R);
			Triangle(Cx, -1.0f, R);
			break;
		case ETransportIcon::ToLast:
			VerticalBar(Cx + R - Bar);
			Triangle(Cx + R - Bar, +1.0f, R);
			Triangle(Cx, +1.0f, R);
			break;
		case ETransportIcon::StepBack:
			VerticalBar(Cx - R * 0.8f);
			Triangle(Cx - R * 0.8f + Bar, -1.0f, R * 1.5f);
			break;
		case ETransportIcon::StepForward:
			VerticalBar(Cx + R * 0.8f - Bar);
			Triangle(Cx + R * 0.8f - Bar, +1.0f, R * 1.5f);
			break;
		case ETransportIcon::Loop:
		{
			const float Pi = 3.14159265f;
			const float A0 = -Pi * 0.35f;
			const float A1 = Pi * 1.25f;
			DrawList->PathArcTo(ImVec2(Cx, Cy), R, A0, A1, 16);
			DrawList->PathStroke(IconCol, ImDrawFlags_None, 1.8f);

			const ImVec2 End(Cx + std::cos(A1) * R, Cy + std::sin(A1) * R);
			const float Head = R * 0.55f;
			const float Tx = -std::sin(A1), Ty = std::cos(A1);
			const float Nx = std::cos(A1), Ny = std::sin(A1);
			DrawList->AddTriangleFilled(
				ImVec2(End.x + Tx * Head, End.y + Ty * Head),
				ImVec2(End.x + Nx * Head * 0.7f, End.y + Ny * Head * 0.7f),
				ImVec2(End.x - Nx * Head * 0.7f, End.y - Ny * Head * 0.7f),
				IconCol);
			break;
		}
		}

		return bPressed;
	}
}

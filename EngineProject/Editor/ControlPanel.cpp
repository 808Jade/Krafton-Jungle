#include "EnginePCH.h"
#include "ControlPanel.h"
#include "../Camera/CameraActor.h"
#include "../Camera/CameraComponent.h"

#include "Input/InputSystem.h"

bool FControlPanel::Init()
{



	return true;
}

void FControlPanel::Tick(float DeltaTime)
{
	if (FInputSystem::IsKeyPressed(EKeyCode::Space))
	{
		static int ModeIndex = 0;
		ModeIndex = (ModeIndex + 1) % 3;
		Gizmo->SetMode(static_cast<EGizmoMode>(ModeIndex));
	}
}

void FControlPanel::AddActor(EPrimitiveType Type)
{
	FTransform Transform;
	AActor* Actor = World->SpawnActor(AActor::StaticClass(), &Transform);
	Actor->AddPrimitiveComponent(Type, Transform);


	ActorNum = World->GetActorNum() - 1;
}


void FControlPanel::OnRender()
{
	ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
	ImGui::Begin("Jungle Control Panel");

	ImGui::Text("Hello Jungle World");
	ImGui::Text("FPS: %.2f (%.0f ms)", 1.0f / DeltaTime, DeltaTime * 1000.0f);

	ImGui::Separator();
	ImGui::SetNextItemWidth(130.0f);
	ImGui::Combo("Actor", &SelectedIndex, Items, IM_ARRAYSIZE(Items));
	ImGui::SameLine();
	ImGui::Text("Primitive");
	//EngineTimer::GetDeltaTime();
	if (ImGui::SmallButton("Spawn")) { AddActor(static_cast<EPrimitiveType>(SelectedIndex)); }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.0f);
	ImGui::InputInt("##N", &ActorNum, 0, 0, ImGuiInputTextFlags_ReadOnly);
	ImGui::SameLine();
	ImGui::Text("Number of spawn");

	ImGui::Separator();
	// 씬 생성 세이브 로드
	ImGui::SetNextItemWidth(165.0f);
	ImGui::InputText("Scene Name", SceneName, IM_ARRAYSIZE(SceneName));
	if (ImGui::Button("New Scene", ImVec2(80.0f, 19.0f))) 
	{
		World->ClearScene();
		World->NewScene(SceneName);
		ActorNum = World->GetActorNum() - 1; 
		if (Callback) Callback();
	}
	if (ImGui::Button("Save Scene", ImVec2(80.0f, 19.0f))) { World->SaveScene(SceneName); ActorNum = World->GetActorNum() - 1; }
	if (ImGui::Button("Load Scene", ImVec2(80.0f, 19.0f)))
	{
		
		if (World->LoadScene(SceneName))
		{
			return;
		}
		ActorNum = World->GetActorNum() - 1;
		if (Callback)Callback();
	}
	ImGui::Separator();
	UCameraComponent* CamCom = World->GetMainCamera()->GetCameraComponent();

	ImGui::Checkbox("Orthogonal", &CamCom->bIsOrthogonal);


	FTransform* transform = CamCom->GetTransform();

	ImGui::SetNextItemWidth(255.0f);
	ImGui::InputFloat("##FOV", &CamCom->FOV);

	ImGui::SetNextItemWidth(80.0f);
	ImGui::InputFloat("##X", &transform->Location.X);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.0f);
	ImGui::InputFloat("##Y", &transform->Location.Y);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.0f);
	ImGui::InputFloat("##Z", &transform->Location.Z);
	ImGui::SameLine();
	ImGui::Text("Camera Location");

	ImGui::SetNextItemWidth(80.0f);
	ImGui::InputFloat("##Pitch", &transform->Rotation.Pitch);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.0f);
	ImGui::InputFloat("##Yaw", &transform->Rotation.Yaw);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.0f);
	ImGui::InputFloat("##Roll", &transform->Rotation.Roll);
	ImGui::SameLine();
	ImGui::Text("Camera Rotation");

	ImGui::Separator();

	GizmoSelectedIndex = static_cast<int32>(Gizmo->GetMode());
	if (ImGui::SetNextItemWidth(100.0f); ImGui::Combo("##GizmoCombo", &GizmoSelectedIndex, GizmoItems, IM_ARRAYSIZE(GizmoItems)))
	{
		Gizmo->SetMode(static_cast<EGizmoMode>(GizmoSelectedIndex));
	}

	if (ImGui::SetNextItemWidth(100.0f); ImGui::Combo("##SpaceCombo", &SpaceSelectedIndex, SpaceItems, IM_ARRAYSIZE(SpaceItems)))
	{
		Gizmo->SetSpace(static_cast<EGizmoSpace>(SpaceSelectedIndex));
	}

	ImGui::End();
}

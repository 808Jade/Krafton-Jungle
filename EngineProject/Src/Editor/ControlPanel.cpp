#include "EnginePCH.h"
#include "ControlPanel.h"
#include "../Camera/CameraActor.h"
#include "../Camera/CameraComponent.h"

#include "GameFramework/Actor/StaticMeshActor.h"
#include "Asset/AssetManager.h"

#include "Input/InputSystem.h"
#include "Core/EngineLog.h"
#include "EditorFileUtils.h"

#include <commdlg.h>

namespace
{
	FString PrimitiveTypeToString(EPrimitiveType Type)
	{
		switch (Type)
		{
		case EPrimitiveType::Sphere:
			return "Sphere";
			break;
		case EPrimitiveType::Cube:
			return "Cube";
			break;
		case EPrimitiveType::Cone:
			return "Cone";
			break;
		case EPrimitiveType::Plane:
			return "Plane";
			break;
		default:
			return "";
			break;
		}
	}
}

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
	FName ActorName(PrimitiveTypeToString(Type));
	AStaticMeshActor* NewActor = World->SpawnActor<AStaticMeshActor>(ActorName, &Transform);
	NewActor->SetPrimitiveType(Type);

	ActorNum = World->GetActorNum();
}

void FControlPanel::OnRender()
{
	ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
	ImGui::Begin("Jungle Control Panel");

	ImGui::Text("FPS: %.2f (%.0f ms)", 1.0f / DeltaTime, DeltaTime * 1000.0f);

	ImGui::Separator();
	ImGui::SetNextItemWidth(130.0f);
	ImGui::Combo("Actor", &SelectedIndex, Items, IM_ARRAYSIZE(Items));
	ImGui::SameLine();
	ImGui::Text("Primitive");
	//EngineTimer::GetDeltaTime();
	if (ImGui::Button("Spawn")) { AddActor(static_cast<EPrimitiveType>(SelectedIndex)); }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.0f);
	ImGui::InputInt("##N", &ActorNum, 0, 0, ImGuiInputTextFlags_ReadOnly);
	ImGui::SameLine();
	ImGui::Text("Number of spawn");

	// Todo: subuv
	if (ImGui::Button("Spawn Particle"))
	{
		FTransform Transform;
		Transform.Location = FVector(5.0f, 0.0f, 0.0f);
		Transform.Scale = FVector(2.0f, 2.0f, 2.0f);

		AParticleActor* ParticleActor = World->SpawnActor<AParticleActor>("Test", &Transform);
		assert(ParticleActor != nullptr);

		// Set atlas image settings
		UParticleSubUVComponent* Particle = ParticleActor->GetParticleComponent();
		Particle->ApplyPrimitiveType(EPrimitiveType::Plane);
		Particle->SetAtlasTexturePath("ParticleAtlas.png");
		Particle->SetSubUVSize(4, 4);
		Particle->SetFrameRate(12.0f);
	}

	ImGui::Separator();

	ImGui::Text("Camera");
	UCameraComponent* CamCom = World->GetMainCamera()->GetCameraComponent();

	bool bOrthogonal = CamCom->GetIsOrthogonal();
	if (ImGui::Checkbox("Orthogonal", &bOrthogonal))
	{
		CamCom->SetIsOrthogonal(bOrthogonal);
	}

	float FOV = CamCom->GetFieldOfView();
	FVector Location = CamCom->GetRelativeLocation();
	FRotator Rotation = CamCom->GetRelativeRotation();

	const FTransform& transform = CamCom->GetTransform();

	ImGui::SetNextItemWidth(255.0f);
	if (ImGui::InputFloat("##FOV", &FOV))
	{
		CamCom->SetFieldOfView(FOV);
	}

	bool bLocationChanged = false;

	ImGui::SetNextItemWidth(80.0f);
	bLocationChanged |= ImGui::InputFloat("##X", &Location.X);
	ImGui::SameLine();

	ImGui::SetNextItemWidth(80.0f);
	bLocationChanged |= ImGui::InputFloat("##Y", &Location.Y);
	ImGui::SameLine();

	ImGui::SetNextItemWidth(80.0f);
	bLocationChanged |= ImGui::InputFloat("##Z", &Location.Z);
	ImGui::SameLine();
	ImGui::Text("Camera Location");

	if (bLocationChanged)
	{
		CamCom->SetRelativeLocation(Location);
	}

	bool bRotationChanged = false;

	ImGui::SetNextItemWidth(80.0f);
	bRotationChanged |= ImGui::InputFloat("##Pitch", &Rotation.Pitch);
	ImGui::SameLine();

	ImGui::SetNextItemWidth(80.0f);
	bRotationChanged |= ImGui::InputFloat("##Yaw", &Rotation.Yaw);
	ImGui::SameLine();

	ImGui::SetNextItemWidth(80.0f);
	bRotationChanged |= ImGui::InputFloat("##Roll", &Rotation.Roll);
	ImGui::SameLine();
	ImGui::Text("Camera Rotation");

	if (bRotationChanged)
	{
		CamCom->SetRelativeRotation(Rotation);
	}

	ImGui::Separator();

	ImGui::Text("Gizmo");
	GizmoSelectedIndex = static_cast<int32>(Gizmo->GetMode());
	if (ImGui::SetNextItemWidth(100.0f); ImGui::Combo("##GizmoCombo", &GizmoSelectedIndex, GizmoItems, IM_ARRAYSIZE(GizmoItems)))
	{
		Gizmo->SetMode(static_cast<EGizmoMode>(GizmoSelectedIndex));
	}

	if (ImGui::SetNextItemWidth(100.0f); ImGui::Combo("##SpaceCombo", &SpaceSelectedIndex, SpaceItems, IM_ARRAYSIZE(SpaceItems)))
	{
		Gizmo->SetSpace(static_cast<EGizmoSpace>(SpaceSelectedIndex));
	}

	ImGui::Separator();

	ImGui::Text("Path Tracker");
	if (ImGui::Button("Start Recording"))
	{
		World->GetPathTracker().SetPathRenderingEnabled(true);
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop Recording"))
	{
		World->GetPathTracker().SetPathRenderingEnabled(false);
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear"))
	{
		World->GetPathTracker().ClearPath();
	}
	if (ImGui::Button("Play Replay"))
	{
		World->GetPathTracker().SetPlaybackEnabled(true);
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop Replay"))
	{
		World->GetPathTracker().SetPlaybackEnabled(false);
	}


	ImGui::End();
}

void FControlPanel::DrawMainMenuBar()
{
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 10.0f));

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Scene")) { CreateNewScene(); }

			if (ImGui::MenuItem("Open Scene...")) { OpenScene(); }

			ImGui::Separator();

			if (ImGui::MenuItem("Save")) { SaveCurrentScene(); }

			if (ImGui::MenuItem("Save As...")) { SaveSceneAs(); }

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
	ImGui::PopStyleVar(2);
}

void FControlPanel::CreateNewScene()
{
	if (!FEditorFileUtils::NewScene(World))
		return;

	ActorNum = World->GetActorNum();

	if (Callback)
		Callback();
}

void FControlPanel::OpenScene()
{
	if (!FEditorFileUtils::LoadScene(World))
		return;

	ActorNum = World->GetActorNum();

	if (Callback)
	{
		Callback();
	}
}

void FControlPanel::SaveCurrentScene()
{
	FEditorFileUtils::SaveScene(World);
}

void FControlPanel::SaveSceneAs()
{
	FEditorFileUtils::SaveSceneAs(World);
}
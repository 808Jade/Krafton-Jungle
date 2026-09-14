#include "EnginePCH.h"
#include "ControlPanel.h"
#include "../Camera/CameraActor.h"
#include "../Camera/CameraComponent.h"

#include "GameFramework/Actor/StaticMeshActor.h"
#include "Asset/AssetManager.h"

#include "Input/InputSystem.h"
#include "Core/EngineLog.h"

#include <commdlg.h>



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
	//FTransform Transform;
	//AStaticMeshActor* NewActor = World->SpawnActor<AStaticMeshActor>(&Transform);
	//NewActor->SetPrimitiveType(Type);
	//AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(&Transform);
	//UPrimitiveComponent* Component = Actor->GetPrimitiveComponent();
	//Component->ApplyPrimitiveType(Type);


	FTransform Transform;
	AStaticMeshActor* NewActor = World->SpawnActor<AStaticMeshActor>(&Transform);
	NewActor->SetPrimitiveType(Type);

	ActorNum = World->GetActorNum();
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

	// Todo: subuv
	if (ImGui::SmallButton("Spawn Particle"))
	{
		FTransform Transform;
		Transform.Location = FVector(5.0f, 0.0f, 0.0f);
		Transform.Scale = FVector(2.0f, 2.0f, 2.0f);

		AParticleActor* ParticleActor = World->SpawnActor<AParticleActor>(&Transform);
		assert(ParticleActor != nullptr);

		// Set atlas image settings
		UParticleSubUVComponent* Particle = ParticleActor->GetParticleComponent();
		Particle->SetAtlasTexturePath("ParticleAtlas.png");
		Particle->SetSubUVSize(4, 4);
		Particle->SetFrameRate(12.0f);
	}

	ImGui::Separator();
	// 씬 생성 세이브 로드
	//ImGui::SetNextItemWidth(165.0f);
	//ImGui::InputText("Scene Name", SceneName, IM_ARRAYSIZE(SceneName));
	


	if (ImGui::Button("New Scene", ImVec2(80.0f, 19.0f)))
	{
		World->ClearScene();
		World->NewScene(SceneName);
		ActorNum = World->GetActorNum();
		if (Callback) Callback();
	}
	ImGui::SameLine();

	if (ImGui::Button("Save Scene", ImVec2(80.0f, 19.0f))) 
	{ 
		FString FilePath = OpenSaveSceneDialog();
		World->SaveScene(FilePath);
		ActorNum = World->GetActorNum(); 
		LOG(Info, "Save Scene Path : {}", FilePath);
	}
	
	DrawSceneLoadCombo(); // Load Scene ImGui


	ImGui::Separator();
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


// Save시 파일탐색기 Open
FString FControlPanel::OpenSaveSceneDialog()
{
	char FilePath[MAX_PATH] = {};

	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFile = FilePath;
	ofn.nMaxFile = MAX_PATH;

	ofn.lpstrFilter = "Scene Files (*.scene)\0*.scene\0";
	ofn.lpstrDefExt = "scene";
	ofn.lpstrInitialDir = DefaultScenePath.c_str();

	ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

	if (GetSaveFileNameA(&ofn)) {
		return FString(FilePath);
	}

	return FString();
}

void FControlPanel::RefreshSceneFiles()
{
	SceneFiles.Reset();
	const FString ScenePath = DefaultScenePath; // Path: Scene/

	if (!std::filesystem::exists(ScenePath)) { 
		LOG(Info, "Scene folder not found: {}\nnow your path is {}", ScenePath.c_str(), std::filesystem::current_path().string());

		LOG(Info, "CurrentPath: {}", std::filesystem::current_path().string());

		return;
	}

	for (const auto& Entry : std::filesystem::directory_iterator(ScenePath))
	{
		if (!Entry.is_regular_file())
			continue;

		SceneFiles.Add(Entry.path().filename().string());
	}

	if (SelectedSceneIndex >= SceneFiles.Num())
	{
		SelectedSceneIndex = -1;
	}
}

void FControlPanel::DrawSceneLoadCombo()
{
	const char* Preview = "Select Scene";

	if (SelectedSceneIndex >= 0 && SelectedSceneIndex <= SceneFiles.Num()) {
		Preview = SceneFiles[SelectedSceneIndex].c_str();
	}

	ImGui::Text("Scene:");
	ImGui::SameLine();

	ImGui::SetNextItemWidth(150.0f);
	bool bComboOpen = ImGui::BeginCombo("##SceneSelect", Preview);

	if (ImGui::IsItemActivated())
	{
		RefreshSceneFiles();
	}
	
	if(bComboOpen)
	{
		for (int i = 0; i < SceneFiles.Num(); ++i)
		{
			bool bSelected = (SelectedSceneIndex == i);

			if (ImGui::Selectable(SceneFiles[i].c_str(), bSelected))
			{
				SelectedSceneIndex = i;
			}

			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}

		ImGui::EndCombo();
	}

	ImGui::SameLine();

	if (ImGui::Button("Load Scene", ImVec2(80.0f, 19.0f)))
	{
		if (SelectedSceneIndex < 0)
			return;

		FString ScenePath = DefaultScenePath + SceneFiles[SelectedSceneIndex];

		RefreshSceneFiles();

		if (!World->LoadScene(ScenePath))
		{
			return;
		}
		LOG(Info, "Load Scene : {}", ScenePath);
		ActorNum = World->GetActorNum();
		if (Callback)Callback();
	}
}
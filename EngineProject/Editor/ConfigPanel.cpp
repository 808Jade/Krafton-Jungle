#include "EnginePCH.h"
#include "ConfigPanel.h"
#include "../Camera/CameraActor.h"
#include "../Camera/CameraComponent.h"

FConfigPanel::~FConfigPanel()
{
	SaveSettings();
}

bool FConfigPanel::Init()
{
	return true;
}

void FConfigPanel::Tick(float DeltaTime)
{
}



void FConfigPanel::OnRender()
{
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGui::Begin("Jungle Config Window");

	// 렌더링 옵션 (Toggles)
	ImGui::Text("Rendering Options");
	ImGui::Separator();

	ImGui::Checkbox("Wireframe Mode", &Settings.bWireframe);
	ImGui::Checkbox("Draw Primitives", &Settings.bDrawPrimitives);
	ImGui::Checkbox("Draw Bounding Box", &Settings.bDrawBoundingBox);
	ImGui::Checkbox("Show Object UUID", &Settings.bShowUUID);
	ImGui::Checkbox("Draw Batch Line / Grid", &Settings.bDrawBatchLine);

	ImGui::Spacing();
	ImGui::Spacing();

	// 에디터 수치 설정 (Values)
	ImGui::Text("Editor Settings");
	ImGui::Separator();

	UCameraComponent* CamCom = World->GetMainCamera()->GetCameraComponent();

	ImGui::SetNextItemWidth(200.0f);
	ImGui::SliderFloat("Camera Rotate Sensitivity", &Settings.MouseSensitivity, 0.1f, 10.0f, "%.2f");
	CamCom->SetMouseSensitivity(Settings.MouseSensitivity);

	ImGui::SetNextItemWidth(200.0f);
	ImGui::SliderFloat("Camera Speed", &Settings.CameraSpeed, 0.1f, 10.0f, "%.2f");
	CamCom->SetCameraSpeed(Settings.CameraSpeed);

	ImGui::SetNextItemWidth(200.0f);
	ImGui::SliderInt("Grid Spacing", &Settings.GridSpacing, 1, 100);

	ImGui::SetNextItemWidth(200.0f);
	ImGui::Checkbox("Draw PS Grid", &Settings.bDrawPSGrid);

	ImGui::Spacing();
	ImGui::Separator();

	// 수동 저장 버튼
	if (ImGui::Button("Save to editor.ini"))
	{
		SaveSettings();
	}
	if (ImGui::Button("Load to editor.ini"))
	{
		LoadSettings();
	}

	ImGui::End();
}

bool FConfigPanel::SaveSettings() const
{
	std::ofstream File("editor.ini");

	if (!File.is_open())
	{
		LOG(Error, "Failed to open editor.ini for saving.");
		return false;
	}

	File << "[Rendering]\n";
	File << "Wireframe=" << Settings.bWireframe << "\n";
	File << "DrawPrimitives=" << Settings.bDrawPrimitives << "\n";
	File << "DrawBoundingBox=" << Settings.bDrawBoundingBox << "\n";
	File << "ShowUUID=" << Settings.bShowUUID << "\n";
	File << "DrawBatchLine=" << Settings.bDrawBatchLine << "\n";
	File << "DrawPSGrid=" << Settings.bDrawPSGrid << "\n";
	File << "\n";

	File << "[Editor]\n";
	File << "CameraMoveSpeed=" << Settings.CameraSpeed << "\n";
	File << "CameraRotateSensitivity=" << Settings.MouseSensitivity << "\n";
	File << "GridSpacing=" << Settings.GridSpacing << "\n";

	File.close();
	return true;
}

bool FConfigPanel::LoadSettings()
{
	std::ifstream File("editor.ini");
	if (!File.is_open())
	{
		return false;
	}

	std::string Line;
	while (std::getline(File, Line))
	{
		if (Line.empty() || Line[0] == ';' || Line[0] == '[') continue;

		std::istringstream Iss(Line);
		std::string Key;
		if (std::getline(Iss, Key, '='))
		{
			std::string ValueStr;
			if (std::getline(Iss, ValueStr))
			{
				if (Key == "Wireframe") Settings.bWireframe = std::stoi(ValueStr);
				else if (Key == "DrawPrimitives") Settings.bDrawPrimitives = std::stoi(ValueStr);
				else if (Key == "DrawBoundingBox") Settings.bDrawBoundingBox = std::stoi(ValueStr);
				else if (Key == "ShowUUID") Settings.bShowUUID = std::stoi(ValueStr);
				else if (Key == "DrawBatchLine") Settings.bDrawBatchLine = std::stoi(ValueStr);
				else if (Key == "DrawPSGrid") Settings.bDrawPSGrid = std::stoi(ValueStr);

				else if (Key == "CameraMoveSpeed") Settings.CameraSpeed = std::stof(ValueStr);
				else if (Key == "CameraRotateSensitivity") Settings.MouseSensitivity = std::stof(ValueStr);
				else if (Key == "GridSpacing") Settings.GridSpacing = std::stof(ValueStr);
			}
		}
	}

	File.close();
	return true;
}
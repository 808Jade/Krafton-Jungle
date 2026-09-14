#pragma once
#include <format>
#include "EditorPanel.h"

// Todo: subuv
#include "GameFramework/Actor/ParticleActor.h"

#include <functional>

using SceneClearCallback = std::function<void()>;

class FControlPanel : public IEditorPanel
{
public:
	bool Init() override;
	void Tick(float DeltaTime)override;
	void OnRender() override;

	inline void SetGizmo(FGizmo* InGizmo) { Gizmo = InGizmo; }
	inline void SetSceneClearCallback(SceneClearCallback InCallback) { Callback = InCallback; }
	inline void SetWorld(UWorld* InWorld) { World = InWorld; }

	float DeltaTime = 1.0f;
	UWorld* World; // SpawnActor MainCamera
	char SceneName[128] = "";

	FShader* Shader;

	void AddActor(EPrimitiveType Type);

	int32 SelectedIndex = 0;
	int32 ActorNum = 0;

	const char* Items[4] =
	{
		"Sphere",
		"Cube",
		"Cone",
		"Plane",
	};

	FGizmo* Gizmo;

	int32 GizmoSelectedIndex = 0;

	const char* GizmoItems[3] =
	{
		"Location",
		"Rotation",
		"Scale"
	};

	int32 SpaceSelectedIndex = 0;

	const char* SpaceItems[2] =
	{
		"Local",
		"World"
	};

	SceneClearCallback Callback = nullptr;



	FString OpenSaveSceneDialog();
	void RefreshSceneFiles();
	void DrawSceneLoadCombo();

private:
	TArray<FString> SceneFiles;
	const FString DefaultScenePath = "Scene/";
	int SelectedSceneIndex = -1;

};


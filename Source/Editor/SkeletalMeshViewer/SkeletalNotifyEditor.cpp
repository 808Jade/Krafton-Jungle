#include "EnginePCH.h"
#include "Editor/SkeletalMeshViewer/SkeletalNotifyEditor.h"
#include "Editor/SkeletalMeshViewer/SkeletalPreviewTabBase.h"
#include "Editor/SkeletalMeshViewer/SkeletalMeshEditorUtils.h"
#include "Animation/AssetAnimInstance.h"
#include "Animation/AnimGraph.h"
#include "SkeletalMeshEditorUtils.h"
#include "Asset/AssetManager.h"
#include "Camera/MinimalViewInfo.h"
#include "Collision/Ray.h"
#include "Engine/SceneManagement.h"
#include "GameFramework/Actor/SkeletalMeshActor.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "GameFramework/Actor/DirectionalLightActor.h"
#include "GameFramework/Actor/SkyLightActor.h"
#include "Engine/World.h"
#include "Input/InputSystem.h"
#include "Render/RenderCommand.h"
#include "Math/EngineMath.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "Render/Material.h"
#include "Render/RenderResourceManager.h"
#include "Components/SkinnedMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Core/Misc/FileHelper.h"
#include "Animation/AnimNotifyFile.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotify_CameraShake.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Engine/Engine.h"
#include "Audio/AudioSystem.h"
#include "Camera/Shake/CameraShakePattern.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectGlobals.h"
#include "Core/Misc/FileHelper.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>

using namespace SkeletalMeshEditorUtils;

TArray<FString> FSkeletalNotifyEditor::CollectUsedNotifyNames() const
{
	TArray<FString> Names;
	const FFbxAssetSet* Assets = Tab.SelectedMesh ? UAssetManager::Get().FindFbxAssets(Tab.SelectedMesh->GetPath()) : nullptr;
	if (!Assets)
	{
		return Names;
	}

	for (const UAnimSequence* Sequence : Assets->Animations)
	{
		if (!Sequence)
		{
			continue;
		}
		for (const FAnimNotifyEvent& Event : Sequence->Notifies)
		{
			if (Event.Notify)
			{
				continue;
			}
			const FString Name = Event.NotifyName.ToString();
			if (!Name.empty() && Name != "None" && Names.Find(Name) == INDEX_NONE)
			{
				Names.Add(Name);
			}
		}
	}

	std::sort(Names.begin(), Names.end());
	return Names;
}

void FSkeletalNotifyEditor::RefreshAudioFileList(const char* Folder)
{
	AudioFileList.Reset();

	std::error_code Error;
	const std::filesystem::path Root = FFileHelper::ToNativePath(Folder);
	if (!std::filesystem::is_directory(Root, Error))
	{
		return;
	}

	for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(Root, Error))
	{
		if (!Entry.is_regular_file())
		{
			continue;
		}

		FString Extension = FFileHelper::ToUtf8Path(Entry.path().extension());
		std::transform(Extension.begin(), Extension.end(), Extension.begin(),
			[](unsigned char C) { return static_cast<char>(std::tolower(C)); });
		if (Extension == ".wav" || Extension == ".mp3" || Extension == ".flac" || Extension == ".ogg")
		{
			const std::u8string GenericPath = Entry.path().lexically_normal().generic_u8string();
			AudioFileList.Add(FString(GenericPath.begin(), GenericPath.end()));
		}
	}
	std::sort(AudioFileList.begin(), AudioFileList.end());
}

void FSkeletalNotifyEditor::SelectPreviewNotify(int32 Index)
{
	SelectedNotifyIndex = Index;
	NotifyNameBuffer[0] = '\0';

	const UAnimSequence* Sequence = Tab.Animation.GetSelectedPreviewSequence();
	if (Sequence && Index >= 0 && Index < Sequence->Notifies.Num())
	{
		const FString Name = Sequence->Notifies[Index].NotifyName.ToString();
		snprintf(NotifyNameBuffer, sizeof(NotifyNameBuffer), "%s", Name.c_str());
	}
}

int32 FSkeletalNotifyEditor::AddPreviewNotify(float Time, float Duration)
{
	UAnimSequence* Sequence = Tab.Animation.GetSelectedPreviewSequence();
	if (!Sequence)
	{
		return -1;
	}

	const float Length = Sequence->GetPlayLength();
	FAnimNotifyEvent Notify;
	Notify.NotifyName = FName("NewNotify");
	Notify.TriggerTime = std::clamp(SnapNotifyTimeToFrame(*Sequence, Time), 0.0f, Length);
	Notify.Duration = std::clamp(Duration, 0.0f, Length - Notify.TriggerTime);
	Sequence->Notifies.Add(Notify);
	bNotifyDirty = true;

	return SortPreviewNotifies(Sequence->Notifies.Num() - 1);
}

void FSkeletalNotifyEditor::RemovePreviewNotify(int32 Index)
{
	UAnimSequence* Sequence = Tab.Animation.GetSelectedPreviewSequence();
	if (!FindPreviewNotify(Sequence, Index))
	{
		return;
	}

	Sequence->RemoveNotifyAt(Index);
	bNotifyDirty = true;
}

void FSkeletalNotifyEditor::SetPreviewNotifyTime(int32 Index, float Time, bool bSnapToFrame)
{
	UAnimSequence* Sequence = Tab.Animation.GetSelectedPreviewSequence();
	FAnimNotifyEvent* Notify = FindPreviewNotify(Sequence, Index);
	if (!Notify)
	{
		return;
	}

	if (bSnapToFrame)
	{
		Time = SnapNotifyTimeToFrame(*Sequence, Time);
	}
	const float MaxTime = std::max(0.0f, Sequence->GetPlayLength() - Notify->Duration);
	Time = std::clamp(Time, 0.0f, MaxTime);

	if (Time != Notify->TriggerTime)
	{
		Notify->TriggerTime = Time;
		bNotifyDirty = true;
	}
}

int32 FSkeletalNotifyEditor::SortPreviewNotifies(int32 KeepIndex)
{
	UAnimSequence* Sequence = Tab.Animation.GetSelectedPreviewSequence();
	if (!Sequence)
	{
		return -1;
	}

	TArray<FAnimNotifyEvent>& Notifies = Sequence->Notifies;
	TArray<int32> Order;
	for (int32 i = 0; i < Notifies.Num(); ++i)
	{
		Order.Add(i);
	}
	std::stable_sort(Order.begin(), Order.end(), [&Notifies](int32 A, int32 B)
	{
		return Notifies[A].TriggerTime < Notifies[B].TriggerTime;
	});

	TArray<FAnimNotifyEvent> Sorted;
	int32 NewKeepIndex = -1;
	for (int32 i = 0; i < Order.Num(); ++i)
	{
		Sorted.Add(Notifies[Order[i]]);
		if (Order[i] == KeepIndex)
		{
			NewKeepIndex = i;
		}
	}
	Notifies = std::move(Sorted);
	return NewKeepIndex;
}

void FSkeletalNotifyEditor::SetPreviewNotifyDuration(int32 Index, float Duration)
{
	UAnimSequence* Sequence = Tab.Animation.GetSelectedPreviewSequence();
	FAnimNotifyEvent* Notify = FindPreviewNotify(Sequence, Index);
	if (!Notify || !CanHaveNotifyDuration(*Notify))
	{
		return;
	}

	const float MaxDuration = std::max(0.0f, Sequence->GetPlayLength() - Notify->TriggerTime);
	Duration = std::clamp(Duration, 0.0f, MaxDuration);

	if (Duration != Notify->Duration)
	{
		Notify->Duration = Duration;
		bNotifyDirty = true;
	}
}

void FSkeletalNotifyEditor::RenamePreviewNotify(int32 Index, const FString& Name)
{
	FAnimNotifyEvent* Notify = FindPreviewNotify(Tab.Animation.GetSelectedPreviewSequence(), Index);
	if (!Notify || Name.empty())
	{
		return;
	}

	const FName NewName(Name);
	if (!(NewName == Notify->NotifyName))
	{
		Notify->NotifyName = NewName;
		bNotifyDirty = true;
	}
}

void FSkeletalNotifyEditor::SetPreviewNotifyClass(int32 Index, UClass* NotifyClass)
{
	UAnimSequence* Sequence = Tab.Animation.GetSelectedPreviewSequence();
	FAnimNotifyEvent* Notify = FindPreviewNotify(Sequence, Index);
	if (!Notify)
	{
		return;
	}

	const UClass* CurrentClass = Notify->Notify ? Notify->Notify->GetClass() : nullptr;
	if (CurrentClass == NotifyClass)
	{
		return;
	}

	UAnimNotify* NewNotify = nullptr;
	if (NotifyClass)
	{
		if (!NotifyClass->IsChildOf(UAnimNotify::StaticClass()))
		{
			return;
		}
		NewNotify = NewObject<UAnimNotify>(Sequence, NotifyClass);
		if (!NewNotify)
		{
			return;
		}
		NewNotify->NormalizeProperties();
	}

	Sequence->SetNotifyObject(Index, NewNotify);
	if (NewNotify && IsNotifyStateClass(NotifyClass))
	{
		if (Notify->Duration <= 0.0f)
		{
			Notify->Duration = std::max(0.0f, std::min(0.25f, Sequence->GetPlayLength() - Notify->TriggerTime));
		}
	}
	else if (NewNotify)
	{
		Notify->Duration = 0.0f;
	}
	bNotifyDirty = true;
}

void FSkeletalNotifyEditor::OnPreviewNotifyPropertyChanged(int32 Index, const FProperty& Property)
{
	FAnimNotifyEvent* Notify = FindPreviewNotify(Tab.Animation.GetSelectedPreviewSequence(), Index);
	if (!Notify || !Notify->Notify)
	{
		return;
	}

	Notify->Notify->NormalizeProperties(&Property);
	bNotifyDirty = true;
}

bool FSkeletalNotifyEditor::SavePreviewNotifies()
{
	if (!Tab.SelectedMesh)
	{
		return false;
	}

	const FFbxAssetSet* Assets = UAssetManager::Get().FindFbxAssets(Tab.SelectedMesh->GetPath());
	if (!Assets)
	{
		Tab.SaveStatusMessage = "Cannot save notifies: mesh was not loaded from an FBX.";
		Tab.SaveStatusTimer = 3.0f;
		return false;
	}

	std::filesystem::path FbxFile = FFileHelper::ToNativePath(Tab.SelectedMesh->GetPath());
	if (FbxFile.extension() == ".bin")
	{
		FbxFile.replace_extension();
	}
	if (!std::filesystem::exists(FbxFile))
	{
		FbxFile = std::filesystem::path("Assets") / FbxFile;
	}

	const FString FbxPath = FFileHelper::ToUtf8Path(FbxFile);
	if (!FAnimNotifyFile::Save(FbxPath, Assets->Animations))
	{
		Tab.SaveStatusMessage = "Failed to save notifies.";
		Tab.SaveStatusTimer = 3.0f;
		return false;
	}

	bNotifyDirty = false;
	Tab.SaveStatusMessage = "Saved notifies: " + FAnimNotifyFile::GetNotifyPath(FbxPath);
	Tab.SaveStatusTimer = 3.0f;
	return true;
}


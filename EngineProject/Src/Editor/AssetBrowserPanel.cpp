#include "EnginePCH.h"
#include "AssetBrowserPanel.h"
#include "Asset/AssetManager.h"

bool FAssetBrowserPanel::Init()
{
	return false;
}

void FAssetBrowserPanel::Tick(float DeltaTime)
{
}

void FAssetBrowserPanel::OnRender()
{
	ImGui::Begin("Asset Browser");
	//if (currentPath != rootPath)
	//{
	//	if (ImGui::Button("Back"))
	//	{
	//		currentPath = currentPath.parent_path();
	//	}
	//}

	float treeViewWidth = 200.0f;
	ImGui::BeginChild("FolderTreeView", ImVec2(treeViewWidth, 0));
	{
		// 루트 노드
		if (ImGui::TreeNodeEx(AssetRootPath.string().c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
			(AssetRootPath == CurrentPath ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_DrawLinesToNodes))
		{
			if (ImGui::IsItemClicked())
			{
				CurrentPath = AssetRootPath;
			}
			RenderFolderTree(AssetRootPath, CurrentPath);
			ImGui::TreePop();
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("AssetGridView");
	{
		// 그리드 레이아웃 계산
		float padding = 16.0f;
		float thumbnailSize = 80.0f;
		float cellSize = thumbnailSize + padding;
		float panelWidth = ImGui::GetContentRegionAvail().x;
		int ColumnCount = static_cast<int>(panelWidth / cellSize);
		if (ColumnCount < 1) ColumnCount = 1;

		// 테이블 기반 그리드 시작
		if (ImGui::BeginTable("AssetGridTable", ColumnCount))
		{
			for (const fs::path& Path : fs::directory_iterator(CurrentPath))
			{
				ImGui::TableNextColumn();

				std::string PathString = Path.generic_string();
				std::string FilenameString = Path.filename().string();

				ImGui::PushID(PathString.c_str()); // 각 위젯에 고유 ID 부여

				UTexture2D* Thumbnail = nullptr;

				// 폴더 또는 파일 아이콘 표시
				if (fs::is_directory(Path))
				{
					Thumbnail = UAssetManager::GetAssetByPath<UTexture2D>(FolderIconPath);
				}

				else // 파일인 경우
				{
					URenderAsset* Asset = UAssetManager::GetAssetByPath<URenderAsset>(PathString);
					if (Asset && Asset->IsA<UTexture2D>())
					{
						Thumbnail = Cast<UTexture2D>(Asset);
					}
					else
					{
						Thumbnail = UAssetManager::GetAssetByPath<UTexture2D>(FileIconPath);
					}
				}
				if (Thumbnail)
				{
					ImGui::ImageButton(PathString.c_str(), (ImTextureID)Thumbnail->GetResource()->GetSRV(), { thumbnailSize, thumbnailSize });
				}

				// 더블 클릭 이벤트 처리
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					if (fs::is_directory(Path))
					{
						CurrentPath /= Path.filename(); // 하위 폴더로 이동
					}
				}

				// 파일 이름 표시
				ImGui::TextWrapped("%s", FilenameString.c_str());

				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
	ImGui::EndChild();
	ImGui::End();
}

void FAssetBrowserPanel::RenderFolderTree(const fs::path& Path, fs::path& SelectedPath)
{
	for (const fs::path& Entry : fs::directory_iterator(Path))
	{
		if (fs::is_directory(Entry))
		{
			const FString FilenameString = Entry.filename().string();

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DrawLinesToNodes;
			if (Entry == SelectedPath) {
				flags |= ImGuiTreeNodeFlags_Selected;
			}

			bool isOpen = ImGui::TreeNodeEx(FilenameString.c_str(), flags);

			if (ImGui::IsItemClicked())
			{
				SelectedPath = Entry;
			}

			if (isOpen) {
				RenderFolderTree(Entry, SelectedPath); // 재귀 호출
				ImGui::TreePop();
			}
		}
	}
}

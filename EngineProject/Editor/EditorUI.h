#pragma once

#include "EditorPanel.h"

class FEditorUI
{
public:
	bool Init();
	void Tick(float DeltaTime);
	void OnRender();

	template <typename T>
	T* AddEditorPanel()
	{
		TUniquePtr<T> newPanel = MakeUnique<T>();
		T* Ret = newPanel.get();
		Ret->Init();
		Panels.push_back(std::move(newPanel));
		
		return Ret;
	}
private:
	TArray<TUniquePtr<IEditorPanel>> Panels;
};
#pragma once

#include <Windows.h>
#include <functional>


class Window
{
public:
	bool Create(HINSTANCE hInstance, int Width, int Height, const wchar_t* Title);
	void ProcessMessage(bool& bIsRunning);

	HWND GetHandle() const { return hWnd;  }

	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }

	LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	bool CheckResized()
	{
		if (bIsResized)
		{
			bIsResized = false;
			return true;
		}
		return false;
	}

private:
	bool bIsResized = false;
	bool bIsInSizeMove = false;

	HWND hWnd = nullptr;

	uint32 Width = 1280;
	uint32 Height = 720;

	
 };


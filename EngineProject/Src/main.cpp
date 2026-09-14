#include "EnginePCH.h"

#include <Windows.h>
#include "Engine/Engine.h"

#include "Math/EngineMath.h"
#include <assert.h>


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    TUniquePtr<Engine> Engine_ = MakeUnique<Engine>();
    if (!Engine_->Init(hInstance)) return -1;

    Engine_->Run();
    Engine_->Shutdown();


    return 0;
}
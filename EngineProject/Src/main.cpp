#include "EnginePCH.h"

#include <Windows.h>
#include "Engine/Engine.h"

#include "Math/EngineMath.h"
#include <assert.h>
#define PI 3.141592f

//int main()
//{
//    HINSTANCE hInstance = GetModuleHandle(nullptr);
//
//    TUniquePtr<Engine> engine = MakeUnique<Engine>();
//    if (!engine->Init(hInstance)) return -1;
//
//    engine->Run();
//    engine->Shutdown();
//}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
    TUniquePtr<Engine> engine = MakeUnique<Engine>();
    if (!engine->Init(hInstance)) return -1;

    engine->Run();
    engine->Shutdown();


    return 0;
}
#include "EnginePCH.h"
#include "InputSystem.h"

bool FInputSystem::IsMouseDown(EMouseButton MouseCode)
{
    return bMouseStates[static_cast<int>(MouseCode)];
}

bool FInputSystem::IsMousePressed(EMouseButton MouseCode)
{
    unsigned int Index = static_cast<unsigned int>(MouseCode);
    return bMouseStates[Index] && !bPrevMouseStates[Index];
}

bool FInputSystem::IsMouseReleased(EMouseButton MouseCode)
{
    unsigned int Index = static_cast<unsigned int>(MouseCode);
    return !bMouseStates[Index] && bPrevMouseStates[Index];
}

bool FInputSystem::IsKeyDown(EKeyCode KeyCode)
{
    return bKeyStates[static_cast<int>(KeyCode)];
}

bool FInputSystem::IsKeyPressed(EKeyCode KeyCode)
{
    unsigned int Index = static_cast<unsigned int>(KeyCode);
    return bKeyStates[Index] && !bPrevKeyStates[Index];
}

bool FInputSystem::IsKeyReleased(EKeyCode KeyCode)
{
    unsigned int Index = static_cast<unsigned int>(KeyCode);
    return !bKeyStates[Index] && bPrevKeyStates[Index];
}

void FInputSystem::OnKeyUp(int VirtualKey)
{
    if (VirtualKey < 256)
    {
        bKeyStates[VirtualKey] = false;
    }
}

void FInputSystem::OnMouseUp(EMouseButton MouseButton)
{
    unsigned int Index = static_cast<int>(MouseButton);
    if (Index < 5)
    {
        bMouseStates[Index] = false;
    }
}


void FInputSystem::OnKeyDown(int VirtualKey)
{
    if (VirtualKey < 256)
    {
        bKeyStates[VirtualKey] = true;
    }
}

void FInputSystem::OnMouseDown(EMouseButton MouseButton)
{
    unsigned int Index = static_cast<int>(MouseButton);
    if (Index < 5)
    {
        bMouseStates[Index] = true;
    }
}

void FInputSystem::OnMouseMove(int32 x, int32 y)
{
    MouseX = x;
    MouseY = y;    
}

void FInputSystem::OnMouseWheelDelta(int32 Wheel)
{
    WheelDelta = Wheel;
}

void FInputSystem::SyncMousePosition(int32 x, int32 y)
{
    // 커서가 창 밖에서 움직인 뒤 돌아왔을 때 델타가 한 번 크게 튀는 것을 막는다.
    MouseX = PrevMouseX = x;
    MouseY = PrevMouseY = y;

    DeltaX = 0;
    DeltaY = 0;
}

void FInputSystem::ClearAllStates()
{
    // 포커스를 잃으면 WM_KEYUP / WM_*BUTTONUP 이 이 창으로 오지 않는다.
    // 현재 상태만 비워서 다음 UpdateInputStates 에서 Released 가 한 번 발생하도록 한다.
    std::memset(bKeyStates, 0, sizeof(bKeyStates));
    std::memset(bMouseStates, 0, sizeof(bMouseStates));

    WheelDelta = 0;
    DeltaX = 0;
    DeltaY = 0;
}

void FInputSystem::UpdateInputStates()
{
    std::memcpy(bPrevKeyStates, bKeyStates, sizeof(bKeyStates));
    std::memcpy(bPrevMouseStates, bMouseStates, sizeof(bMouseStates));
    
    DeltaX = MouseX - PrevMouseX;
    DeltaY = MouseY - PrevMouseY;

    PrevMouseX = MouseX;
    PrevMouseY = MouseY;
}


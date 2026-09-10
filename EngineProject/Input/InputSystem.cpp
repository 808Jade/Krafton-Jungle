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

void FInputSystem::UpdateInputStates()
{
    std::memcpy(bPrevKeyStates, bKeyStates, sizeof(bKeyStates));
    std::memcpy(bPrevMouseStates, bMouseStates, sizeof(bMouseStates));
    
    DeltaX = PrevMouseX - MouseX;
    DeltaY = PrevMouseY - MouseY;

    PrevMouseX = MouseX;
    PrevMouseY = MouseY;
}


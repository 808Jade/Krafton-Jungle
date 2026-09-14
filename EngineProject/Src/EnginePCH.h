#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif // !NOMINMAX
#include <Windows.h>
#include <windowsx.h>

#include <wrl.h> 
#include <d3d11.h>
#include <dxgidebug.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler") 
#pragma comment(lib, "DXGI") 

using namespace Microsoft::WRL;

#include <iostream>
#include <fstream>
#include "Container/Containers.h"
#include "Core/EngineString.h"
#include "ObjectSystem/Casts.h"
#include "Math/EngineMath.h"
#include "Core/EngineLog.h"

#include "json.hpp"
using json = nlohmann::ordered_json;



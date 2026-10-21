#pragma once

#include <cstdint>

enum class EShaderType
{
	Vertex,
	Pixel,
	Compute,
};

enum class ERenderingPath
{
    Forward,
    Deferred
};

enum class ELightingModel
{
    Gouraud,
    Lambert,
    Phong,
    Unlit // 조명과 그림자 없이 Base Color만 출력한다
};

enum class ELightCullingMode
{
    Off = 0,
    Tiled = 1,
    Clustered = 2
};

enum class EPcfMode : uint32_t
{
    Off = 0,
    On = 1, // 3x3
    Kernel4x4 = 2,
    Kernel5x5 = 3,
    PCSS = 4
};

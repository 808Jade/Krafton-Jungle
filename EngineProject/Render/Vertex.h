#pragma once

#include <d3d11.h>

struct FVertex
{
	FVector Position;
	FVector4 Color;

	static std::vector<D3D11_INPUT_ELEMENT_DESC> GetLayout() 
	{
		return
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
	}
};
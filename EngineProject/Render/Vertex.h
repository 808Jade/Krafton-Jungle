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

struct FTextVertex
{
	FVector Position;
	FVector2 UV;

	static std::vector<D3D11_INPUT_ELEMENT_DESC> GetLayout()
	{
		return
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
	}
};

// Todo: subuv
// Particle은 위치 + UV만 필요하므로 기존 FTextVertex와 같은 구조를 별도 타입으로 사용한다.
struct FParticleVertex
{
	FVector Position;
	FVector2 UV;

	static std::vector<D3D11_INPUT_ELEMENT_DESC> GetLayout()
	{
		return
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
	}
};

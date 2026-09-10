#pragma once
#include <wrl/client.h>
#include "../Core/Types.h"

struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;

enum class EShaderType
{
	Vertex,
	Pixel
};

struct FShader
{
	// 모든 셰이더 구조체는 ID3D11DeviceChild 의 자식들임
	Microsoft::WRL::ComPtr<ID3D11VertexShader> VertexShader = nullptr;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> PixelShader = nullptr;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> InputLayout = nullptr;
	 
	uint32 Stride;
};

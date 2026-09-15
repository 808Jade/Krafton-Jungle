#pragma once

#include "Shader.h"


struct FPipelineState
{
	FPipelineState();
	~FPipelineState() = default;

	FShader* Shader = nullptr;
	D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	ERasterizerState         RasterizerState = ERasterizerState::SolidBack;
	EBlendState              BlendState = EBlendState::Opaque;
	EDepthStencilState       DepthStencilState = EDepthStencilState::Default;
};
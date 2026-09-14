#pragma once

#include "Renderer.h"

class RenderCommand
{
	inline static void Init(FRenderer* InRenderer) { Renderer = InRenderer; };

	inline static void BindPipelineState();

	inline static void BindMesh(UStaticMesh* Mesh);

	inline static void Draw();
	inline static void DrawIndexed();

private:
	static FRenderer* Renderer;
};
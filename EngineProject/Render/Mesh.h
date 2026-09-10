#pragma once

#include "Render/Buffer.h"

struct FMesh
{
	TSharedPtr<FVertexBuffer> VertexBuffer;
	TSharedPtr<FIndexBuffer> IndexBuffer;
};
#pragma once

#include "Asset/RenderAsset.h"
#include "Render/Buffer.h"

class UStaticMesh : public URenderAsset
{
public:
	virtual ~UStaticMesh() override {}


	TSharedPtr<FVertexBuffer> VertexBuffer;
	TSharedPtr<FIndexBuffer> IndexBuffer;
};
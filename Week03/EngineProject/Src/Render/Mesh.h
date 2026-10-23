#pragma once

#include "Asset/RenderAsset.h"
#include "Render/Buffer.h"

class UStaticMesh : public URenderAsset
{
	DECLARE_CLASS(UStaticMesh, URenderAsset)
public:
	virtual ~UStaticMesh() override;


	TUniquePtr<FVertexBuffer> VertexBuffer;
	TUniquePtr<FIndexBuffer> IndexBuffer;
};
#pragma once

#include "RenderPacket.h"
#include "Texture2D.h"
#include "Text/Font.h"


#include "RenderingInfo.h"

class UCameraComponent;

class FRenderer
{
	friend class UTexture2D;
public:

	bool Init();

	UStaticMesh* CreateMesh(const FStaticMeshData& InMeshData);
	UStaticMesh* CreateMesh(TUniquePtr<FVertexBuffer> VertexBuffer, TUniquePtr<FIndexBuffer> IndexBuffer);

	void RenderAll(TQueue<FRenderPacket>& InQueue, UCameraComponent* CameraComponent);

private:
	TUniquePtr<FConstantBuffer> CB;
	TUniquePtr<FConstantBuffer> Temp;

	D3D11_VIEWPORT ViewportInfo;

	uint32 Width;
	uint32 Height;

	FLOAT ClearColor[4] = { 0.3f, 0.3f, 0.3f, 1.0f };


};

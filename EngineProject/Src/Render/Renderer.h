#pragma once

#include "RenderPacket.h"
#include "Texture2D.h"
#include "Text/FontAtlas.h"


#include "RenderingInfo.h"




class FRenderer
{
	friend class UTexture2D;
public:

	bool Init();

	UStaticMesh* CreateMesh(const FStaticMeshData& InMeshData);
	UStaticMesh* CreateMesh(TSharedPtr<FVertexBuffer> VertexBuffer, TSharedPtr<FIndexBuffer> IndexBuffer);

	void RenderAll(TQueue<FRenderPacket>& InQueue, FMatrix VP);

private:


	// Camera 
	struct FConstants
	{
		FMatrix MVP;
	};

	struct FSubUVConstants
	{
		float CurrentFrame;
		float AtlasRowSize;
		float AtlasColSize;
		float Padding = 0.0f;
	};

	TUniquePtr<FConstantBuffer> CB;
	TUniquePtr<FConstantBuffer> Temp;

	D3D11_VIEWPORT ViewportInfo;

	uint32 Width;
	uint32 Height;

	FLOAT ClearColor[4] = { 0.3f, 0.3f, 0.3f, 1.0f };


};

#include "EnginePCH.h"
#include "Renderer.h"
#include "Shader.h"
#include "Mesh.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void FRenderer::BeginFrame()
{
	DeviceContext->ClearRenderTargetView(FrameBufferRTV.Get(), ClearColor);
	DeviceContext->ClearDepthStencilView(FrameBufferDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	DeviceContext->OMSetRenderTargets(1, FrameBufferRTV.GetAddressOf(), FrameBufferDSV.Get());
	DeviceContext->OMSetDepthStencilState(DepthStencilState.Get(), 0);
	DeviceContext->RSSetViewports(1, &ViewportInfo);
	DeviceContext->RSSetState(RasterizerState.Get());

}

void FRenderer::EndFrame()
{
	SwapChain->Present(1, 0);
}

bool FRenderer::Init(HWND hWindow)
{
	CreateDeviceAndSwapChain(hWindow);

	CreateFrameBuffer();

	CreateRasterizerState();
	CreateDepthStencilBufferAndState();
	CreateConstantBuffer();

	D3D11_SAMPLER_DESC Desc = {};
	Desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	Desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	Desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	Desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	Desc.MipLODBias = 0.0f;
	Desc.MaxAnisotropy = 1;
	Desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	Desc.MinLOD = 0.0f;
	Desc.MaxLOD = D3D11_FLOAT32_MAX;

	HRESULT hr = Device->CreateSamplerState(&Desc, &DefaultSamplerState);

	// 임시 셰이더 프로그램 컴파일 로직
	/*D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{"POSITION" , 0 , DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	FShader* shader = CreateShader(L"Shader/DefaultShader.hlsl", layout, 2);

	DeviceContext->VSSetShader(shader->VertexShader.Get(), nullptr, 0);
	DeviceContext->PSSetShader(shader->PixelShader.Get(), nullptr, 0);
	DeviceContext->IASetInputLayout(shader->InputLayout.Get());*/
	return true;
}


void FRenderer::CreateDeviceAndSwapChain(HWND hWindow)
{
	D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_0 };

	DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
	SwapChainDesc.BufferDesc.Width = 0;
	SwapChainDesc.BufferDesc.Height = 0;

	SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	SwapChainDesc.SampleDesc.Count = 1;
	SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc.BufferCount = 2;
	SwapChainDesc.OutputWindow = hWindow;
	SwapChainDesc.Windowed = TRUE;
	SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	UINT CreateFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;   // 기존 플래그 유지
#if defined(_DEBUG)
	CreateFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
		CreateFlags,
		FeatureLevels, ARRAYSIZE(FeatureLevels), D3D11_SDK_VERSION,
		&SwapChainDesc, SwapChain.GetAddressOf(), Device.GetAddressOf(), nullptr, DeviceContext.GetAddressOf());

	SwapChain->GetDesc(&SwapChainDesc);

	ViewportInfo = { 0.0f, 0.0f,
		(float)SwapChainDesc.BufferDesc.Width, (float)SwapChainDesc.BufferDesc.Height,
		0.0f, 1.0f };
}

void FRenderer::CreateFrameBuffer()
{
	SwapChain->GetBuffer(0, IID_PPV_ARGS(FrameBuffer.GetAddressOf()));
	Device->CreateRenderTargetView(FrameBuffer.Get(), nullptr, FrameBufferRTV.GetAddressOf());
}


void FRenderer::CreateRasterizerState()
{
	D3D11_RASTERIZER_DESC RasterizerDesc = {};
	RasterizerDesc.FillMode = D3D11_FILL_SOLID;
	RasterizerDesc.CullMode = D3D11_CULL_BACK;

	Device->CreateRasterizerState(&RasterizerDesc, &RasterizerState);
}

void FRenderer::CreateDepthStencilBufferAndState()
{
	D3D11_TEXTURE2D_DESC DepthDesc{};

	DepthDesc.Width = ViewportInfo.Width;
	DepthDesc.Height = ViewportInfo.Height;

	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = 1;
	DepthDesc.Format = DXGI_FORMAT_D32_FLOAT;	// 24비트 깊이, 8비트 스텐실
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.SampleDesc.Quality = 0;
	DepthDesc.Usage = D3D11_USAGE_DEFAULT;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	DepthDesc.CPUAccessFlags = 0;
	DepthDesc.MiscFlags = 0;
	HRESULT hr = Device->CreateTexture2D(&DepthDesc, NULL, DepthStencilBuffer.GetAddressOf());

	Device->CreateDepthStencilView(DepthStencilBuffer.Get(), nullptr, FrameBufferDSV.GetAddressOf());

	D3D11_DEPTH_STENCIL_DESC DepthStencilDesc{};

	// Depth test Paramiter
	DepthStencilDesc.DepthEnable = true;
	DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

	// Stencil test Paramiter
	DepthStencilDesc.StencilEnable = false;
	DepthStencilDesc.StencilReadMask = 0xFF;
	DepthStencilDesc.StencilWriteMask = 0xFF;

	DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	DepthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	Device->CreateDepthStencilState(&DepthStencilDesc, DepthStencilState.GetAddressOf());

	D3D11_DEPTH_STENCIL_DESC disabledDesc = {};
	disabledDesc.DepthEnable = FALSE;
	disabledDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	Device->CreateDepthStencilState(&disabledDesc, &DepthDisabledState);
}

void FRenderer::CreateAlphaBlendState()
{
	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	Device->CreateBlendState(&blendDesc, AlphaBlendState.GetAddressOf());
}

void FRenderer::CreateConstantBuffer()
{
	D3D11_BUFFER_DESC constantbufferdesc = {};

	constantbufferdesc.ByteWidth = sizeof(FConstants) + 0xf & 0xfffffff0;
	constantbufferdesc.Usage = D3D11_USAGE_DYNAMIC;	// will be updated from CPU every frame
	constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	constantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	Device->CreateBuffer(&constantbufferdesc, nullptr, &ConstantBuffer);
}

TSharedPtr<FFontAtlas> FRenderer::LoadFontAtlas(const FString& JsonPath, const FString& AtlasTexturePath)
{
	TSharedPtr<FFontAtlas> Atlas = MakeShared<FFontAtlas>();

	if (!LoadFontAtlasJson(JsonPath, Atlas.get()))
	{
		return nullptr;
	}

	Atlas->AtlasTexture = CreateShaderResource(AtlasTexturePath);
	if (!Atlas->AtlasTexture)
	{
		return nullptr;
	}

	return Atlas;
}

TSharedPtr<FShader> FRenderer::CreateShader(const wchar_t* FileName, D3D11_INPUT_ELEMENT_DESC* InLayoutDesc, size_t InLayoutSize)
{
	TSharedPtr<FShader> Shader = MakeShared<FShader>();

	ID3DBlob* VertexShaderCSO;
	ID3DBlob* ErrorBlob;
	HRESULT hr = D3DCompileFromFile(FileName, nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &VertexShaderCSO, &ErrorBlob);

	if (FAILED(hr))
	{
		if (ErrorBlob)
		{
			OutputDebugStringA((char*)ErrorBlob->GetBufferPointer());
			ErrorBlob->Release();
		}
		assert(false && "Vertex shader compile failed");
		return nullptr; // 혹은 적절한 실패 처리
	}

	hr = Device->CreateVertexShader(VertexShaderCSO->GetBufferPointer(), VertexShaderCSO->GetBufferSize(), nullptr, Shader->VertexShader.GetAddressOf());

	ID3DBlob* PixelShaderCSO;
	D3DCompileFromFile(FileName, nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &PixelShaderCSO, nullptr);
	Device->CreatePixelShader(PixelShaderCSO->GetBufferPointer(), PixelShaderCSO->GetBufferSize(), nullptr, Shader->PixelShader.GetAddressOf());

	if (InLayoutSize > 0)
	{
		hr = Device->CreateInputLayout(InLayoutDesc, InLayoutSize,
			VertexShaderCSO->GetBufferPointer(), VertexShaderCSO->GetBufferSize(), &(Shader->InputLayout));
	}

	VertexShaderCSO->Release();
	PixelShaderCSO->Release();

	return Shader;
}

TSharedPtr<FShader> FRenderer::CreateShader(const wchar_t* FileName, std::vector<D3D11_INPUT_ELEMENT_DESC> Layouts)
{
	return CreateShader(FileName, Layouts.data(), Layouts.size());
}

UStaticMesh* FRenderer::CreateMesh(const FStaticMeshData& InMeshData)
{
	UStaticMesh* Mesh = FObjectFactory::ConstructObject<UStaticMesh>();

	Mesh->VertexBuffer = CreateVertexBuffer(InMeshData.Vertices.GetData(), sizeof(FVertex) * InMeshData.Vertices.Num(), sizeof(FVertex));
	Mesh->IndexBuffer = CreateIndexBuffer(InMeshData.Indices.GetData(), InMeshData.Indices.Num());

	return Mesh;
}


UStaticMesh* FRenderer::CreateMesh(TSharedPtr<FVertexBuffer> VertexBuffer, TSharedPtr<FIndexBuffer> IndexBuffer)
{
	UStaticMesh* Mesh = FObjectFactory::ConstructObject<UStaticMesh>();

	Mesh->VertexBuffer = VertexBuffer;
	Mesh->IndexBuffer = IndexBuffer;

	return Mesh;
}



UTexture2D* FRenderer::CreateShaderResource(const FString& InPath)
{
	//UTexture2D* Texture2D = FObjectFactory::ConstructObject<UTexture2D>();

	//std::wstring WidePath(InPath.begin(), InPath.end());
	//ComPtr<ID3D11Resource> Resource;
	//HRESULT hr = DirectX::CreateWICTextureFromFile(Device.Get(), WidePath.c_str(), Resource.GetAddressOf(), Texture2D->ShaderResourceView.GetAddressOf());
	//if (FAILED(hr))
	//{
	//	delete Texture2D;
	//	return nullptr;
	//}

	//Resource.As(&Texture2D->Texture);

	//return Texture2D;

		// 1. 이미지 로드 (강제 RGBA 4채널)
	stbi_set_flip_vertically_on_load(false); // D3D는 UV 원점이 좌상단 → 뒤집지 않음
	int Width = 0, Height = 0, Channels = 0;
	unsigned char* Pixels = stbi_load(InPath.c_str(), &Width, &Height, &Channels, 4);
	if (!Pixels)
	{
		OutputDebugStringA(("[Texture] stbi_load failed: " + InPath + " (" + stbi_failure_reason() + ")\n").c_str());
		return nullptr;
	}

	std::vector<uint8_t> PixelVec(Width * Height * Channels);

	std::copy(Pixels, Pixels + Width * Height * Channels, PixelVec.data());

	// 2. Texture2D 생성
	D3D11_TEXTURE2D_DESC Desc = {};
	Desc.Width = static_cast<UINT>(Width);
	Desc.Height = static_cast<UINT>(Height);
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // MSDF는 반드시 UNORM (sRGB 금지)
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA InitData = {};
	InitData.pSysMem = Pixels;
	InitData.SysMemPitch = static_cast<UINT>(Width) * 4;

	ComPtr<ID3D11Texture2D> Texture;
	HRESULT hr = Device->CreateTexture2D(&Desc, &InitData, Texture.GetAddressOf());
	stbi_image_free(Pixels); // GPU로 복사 끝났으니 바로 해제
	if (FAILED(hr))
	{
		return nullptr;
	}

	// 3. SRV 생성
	ComPtr<ID3D11ShaderResourceView> SRV;
	hr = Device->CreateShaderResourceView(Texture.Get(), nullptr, SRV.GetAddressOf());
	if (FAILED(hr))
	{
		return nullptr;
	}

	// 4. 전부 성공한 뒤에 오브젝트 생성
	UTexture2D* Texture2D = FObjectFactory::ConstructObject<UTexture2D>();
	Texture2D->Texture = Texture;
	Texture2D->ShaderResourceView = SRV;

	return Texture2D;
}

UTexture2D* FRenderer::CreateRenderTarget(const FString& InPath)
{
	// TODO:
	return nullptr;
}

TSharedPtr<FVertexBuffer> FRenderer::CreateVertexBuffer(const void* InVertices, uint32 InSize, uint32 Stride)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;          // 나중에 규모 커지면 immutable
	Desc.ByteWidth = InSize;
	Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	TSharedPtr<FVertexBuffer> Buffer = MakeShared<FVertexBuffer>(Device.Get(), Desc, Stride);

	UpdateVertexBufferData(Buffer.get(), InVertices, InSize);

	return Buffer;
}

TSharedPtr<FIndexBuffer> FRenderer::CreateIndexBuffer(const uint32* InIndices, uint32 IndexCount)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;          // 나중에 규모 커지면 immutable
	Desc.ByteWidth = sizeof(uint32) * IndexCount;
	Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	TSharedPtr<FIndexBuffer> Buffer = MakeShared<FIndexBuffer>(Device.Get(), Desc, IndexCount);

	UpdateIndexBufferData(Buffer.get(), InIndices, sizeof(uint32) * IndexCount);

	return Buffer;
}

TSharedPtr<FVertexBuffer> FRenderer::CreateDynamicVertexBuffer(uint32 MaxSize, uint32 Stride)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.ByteWidth = MaxSize;
	Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	TSharedPtr<FVertexBuffer> Buffer = MakeShared<FVertexBuffer>(Device.Get(), Desc, Stride);

	return Buffer;
}

TSharedPtr<FIndexBuffer> FRenderer::CreateDynamicIndexBuffer(uint32 MaxIndexCount)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.ByteWidth = sizeof(uint32) * MaxIndexCount;
	Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	TSharedPtr<FIndexBuffer> Buffer = MakeShared<FIndexBuffer>(Device.Get(), Desc, MaxIndexCount);

	return Buffer;
}

TSharedPtr<FConstantBuffer> FRenderer::CreateConstantBuffer(uint32 BufferSize)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.ByteWidth = BufferSize;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	TSharedPtr<FConstantBuffer> Buffer = MakeShared<FConstantBuffer>(Device.Get(), Desc);

	return Buffer;
}

void FRenderer::UpdateVertexBufferData(FVertexBuffer* InBuffer, const void* Data, uint32 DataSize)
{
	ID3D11Buffer* Buffer = InBuffer->GetBuffer();

	D3D11_MAPPED_SUBRESOURCE MappedResource;
	DeviceContext->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	std::memcpy(MappedResource.pData, Data, DataSize);
	DeviceContext->Unmap(Buffer, 0);
}

void FRenderer::UpdateIndexBufferData(FIndexBuffer* InBuffer, const void* Data, uint32 DataSize)
{
	ID3D11Buffer* Buffer = InBuffer->GetBuffer();

	D3D11_MAPPED_SUBRESOURCE MappedResource;
	DeviceContext->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	std::memcpy(MappedResource.pData, Data, DataSize);
	DeviceContext->Unmap(Buffer, 0);
}

void FRenderer::UpdateConstantBufferData(FConstantBuffer* InBuffer, const void* Data, uint32 DataSize)
{
	ID3D11Buffer* Buffer = InBuffer->GetBuffer();

	D3D11_MAPPED_SUBRESOURCE MappedResource;
	DeviceContext->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	std::memcpy(MappedResource.pData, Data, DataSize);
	DeviceContext->Unmap(Buffer, 0);
}

void FRenderer::UpdateConstantBuffer(const FMatrix& MVP)
{
	if (ConstantBuffer)
	{
		D3D11_MAPPED_SUBRESOURCE constantbufferMSR;
		FConstants constants;
		DeviceContext->Map(ConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);
		FConstants* constant = (FConstants*)constantbufferMSR.pData;
		{
			FMatrix TransMVP = MVP.GetTransposed();
			constant->MVP = TransMVP;
		}
		DeviceContext->Unmap(ConstantBuffer.Get(), 0);
		DeviceContext->VSSetConstantBuffers(0, 1, ConstantBuffer.GetAddressOf());
	}
}

void FRenderer::BindVertexBuffer(FVertexBuffer* VertexBuffer)
{
	if (VertexBuffer == nullptr)
	{
		ID3D11Buffer* NullBuffer = nullptr;
		uint32 NullStride = 0;
		uint32 NullOffset = 0;
		DeviceContext->IASetVertexBuffers(0, 1, &NullBuffer, &NullStride, &NullOffset);
		return;
	}

	uint32 offset = 0;
	uint32 Stride = VertexBuffer->GetStride();
	ID3D11Buffer* Buffer = VertexBuffer->GetBuffer();
	DeviceContext->IASetVertexBuffers(0, 1, &Buffer, &Stride, &offset);
}

void FRenderer::BindIndexBuffer(FIndexBuffer* IndexBuffer)
{
	DeviceContext->IASetIndexBuffer(IndexBuffer->GetBuffer(), DXGI_FORMAT_R32_UINT, 0);
}

void FRenderer::BindConstantBuffer(uint32 Slot, FConstantBuffer* ConstantBuffer, EShaderBindFlagBits FlagBits)
{
	ID3D11Buffer* Buffer = ConstantBuffer->GetBuffer();
	if (FlagBits & EShaderBindFlagBits::Vertex)
		DeviceContext->VSSetConstantBuffers(Slot, 1, &Buffer);
	if (FlagBits & EShaderBindFlagBits::Pixel)
		DeviceContext->PSSetConstantBuffers(Slot, 1, &Buffer);
}

void FRenderer::BindTexture(uint32 Slot, UTexture2D* Texture2D, EShaderBindFlagBits FlagBits)
{
	if (FlagBits & EShaderBindFlagBits::Vertex)
	{
		DeviceContext->VSSetShaderResources(Slot, 1, Texture2D->ShaderResourceView.GetAddressOf());
		DeviceContext->VSSetSamplers(0, 1, DefaultSamplerState.GetAddressOf());
	}

	if (FlagBits & EShaderBindFlagBits::Pixel)
	{
		DeviceContext->PSSetShaderResources(Slot, 1, Texture2D->ShaderResourceView.GetAddressOf());
		DeviceContext->PSSetSamplers(0, 1, DefaultSamplerState.GetAddressOf());
	}
}

void FRenderer::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology)
{
	DeviceContext->IASetPrimitiveTopology(Topology);
}

void FRenderer::SetDepthStencilEnabled(bool bEnabled)
{
	DeviceContext->OMSetDepthStencilState(bEnabled ? DepthStencilState.Get() : DepthDisabledState.Get(), 0);
}

void FRenderer::SetBlendStateEnabled(bool bEnabled)
{
	if (bEnabled)
	{
		float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		DeviceContext->OMSetBlendState(AlphaBlendState.Get(), blendFactor, 0xffffffff);
	}
	else
	{
		DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
	}
}

void FRenderer::BindShader(FShader* InShader)
{
	DeviceContext->VSSetShader(InShader->VertexShader.Get(), nullptr, 0);
	DeviceContext->PSSetShader(InShader->PixelShader.Get(), nullptr, 0);
	DeviceContext->IASetInputLayout(InShader->InputLayout.Get());
}

void FRenderer::BindMesh(UStaticMesh* InMesh)
{
	BindVertexBuffer(InMesh->VertexBuffer.get());
	BindIndexBuffer(InMesh->IndexBuffer.get());
}

void FRenderer::Draw(uint32 VertexCount, uint32 StartIndexLocation)
{
	DeviceContext->Draw(VertexCount, 0);
}

void FRenderer::DrawIndexed(uint32 IndexCount, uint32 StartIndexLocation, int32 BaseVertexLocation)
{
	DeviceContext->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
}



//void FRenderer::Prepare()
//{
//	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//	DeviceContext->OMSetRenderTargets(1, FrameBufferRTV.GetAddressOf(), nullptr);
//	DeviceContext->RSSetViewports(1, &ViewportInfo);
//	DeviceContext->RSSetState(RasterizerState.Get());
//	DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffff'ffff);
//
//
//}


void FRenderer::RenderAll(TQueue<FRenderPacket>& InQueue, FMatrix VP)
{
	while (true)
	{
		if (InQueue.IsEmpty())
		{
			return;
		}

		FRenderPacket rp = InQueue.Peek();

		BindShader(rp.shader);
		BindMesh(rp.mesh);

		// rp.Transform 과 Camera VP 행렬 곱
		// 행렬곱의 결과 (MVP Matrix) Constant Buffer 업데이트 필요
		FMatrix MVP;
		MVP = rp.model * VP;
		UpdateConstantBuffer(MVP);
		DrawIndexed(rp.mesh->IndexBuffer->GetIndexCount());

		InQueue.Dequeue();
	}
}

void FRenderer::Shutdown()
{
	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	if (DeviceContext)
	{
		DeviceContext->ClearState();
		DeviceContext->Flush();
	}
}

void FRenderer::Resize(int32 InWidth, int32 InHeight)
{
	// 기존 RTV 해제
	FrameBuffer.Reset();
	FrameBufferRTV.Reset();
	DepthStencilBuffer.Reset();
	FrameBufferDSV.Reset();

	// SwapChain 크기 변경
	SwapChain->ResizeBuffers(
		0,
		InWidth,
		InHeight,
		DXGI_FORMAT_UNKNOWN,
		0
	);

	SwapChain->GetBuffer(
		0,
		IID_PPV_ARGS(&FrameBuffer)
	);

	Device->CreateRenderTargetView(
		FrameBuffer.Get(),
		nullptr,
		&FrameBufferRTV
	);

	D3D11_TEXTURE2D_DESC DepthDesc{};

	DepthDesc.Width = InWidth;
	DepthDesc.Height = InHeight;

	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = 1;
	DepthDesc.Format = DXGI_FORMAT_D32_FLOAT;	// 24비트 깊이, 8비트 스텐실
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.SampleDesc.Quality = 0;
	DepthDesc.Usage = D3D11_USAGE_DEFAULT;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	DepthDesc.CPUAccessFlags = 0;
	DepthDesc.MiscFlags = 0;
	HRESULT hr = Device->CreateTexture2D(&DepthDesc, NULL, DepthStencilBuffer.GetAddressOf());

	Device->CreateDepthStencilView(DepthStencilBuffer.Get(), nullptr, FrameBufferDSV.GetAddressOf());

	ViewportInfo.TopLeftX = 0.0f;
	ViewportInfo.TopLeftY = 0.0f;
	ViewportInfo.Width = static_cast<float>(InWidth);
	ViewportInfo.Height = static_cast<float>(InHeight);
	ViewportInfo.MinDepth = 0.0f;
	ViewportInfo.MaxDepth = 1.0f;

	//DeviceContext->RSSetViewports(1, &Viewport);


}
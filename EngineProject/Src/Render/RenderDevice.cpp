#include "EnginePCH.h"
#include "RenderDevice.h"

#include "Buffer.h"
#include "Texture2D.h"

FRenderDevice::FRenderDevice()
{
	uint32 CreationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
	// If the project is in a debug build, enable debugging via SDK Layers.
	CreationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL FeatureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_HARDWARE;
	HRESULT hr = D3D11CreateDevice(
		nullptr,
		driverType,
		nullptr,
		CreationFlags,
		FeatureLevels,
		ARRAYSIZE(FeatureLevels),
		D3D11_SDK_VERSION,
		Device.GetAddressOf(),
		&FeatureLevel,
		DeviceContext.GetAddressOf());

	if (FAILED(hr))
	{
		LOG(Error, "Failed to Create D3D11Device & DeviceContext!");
	}

	ComPtr<IDXGIDevice> DXGIDevice;
	Device->QueryInterface(IID_PPV_ARGS(DXGIDevice.GetAddressOf()));
	ComPtr<IDXGIAdapter> DXGIAdapter;
	DXGIDevice->GetParent(IID_PPV_ARGS(DXGIAdapter.GetAddressOf()));
	DXGIAdapter->GetParent(IID_PPV_ARGS(DXGIFactory.GetAddressOf()));

	CreateStates();
}

FRenderDevice::~FRenderDevice()
{
}


TUniquePtr<FVertexBuffer> FRenderDevice::CreateStaticVertexBuffer(const void* InVertices, uint32 InSize, uint32 Stride)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_IMMUTABLE;          // 나중에 규모 커지면 immutable
	Desc.ByteWidth = InSize;
	Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	TUniquePtr<FVertexBuffer> Buffer = MakeUnique<FVertexBuffer>(Device.Get(), Desc, Stride, InVertices);

	return Buffer;
}

TUniquePtr<FIndexBuffer> FRenderDevice::CreateStaticIndexBuffer(const uint32* InIndices, uint32 IndexCount)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.ByteWidth = sizeof(uint32) * IndexCount;
	Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	TUniquePtr<FIndexBuffer> Buffer = MakeUnique<FIndexBuffer>(Device.Get(), Desc, IndexCount, InIndices);

	return Buffer;
}

TUniquePtr<FVertexBuffer> FRenderDevice::CreateDynamicVertexBuffer(uint32 MaxSize, uint32 Stride)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.ByteWidth = MaxSize;
	Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	TUniquePtr<FVertexBuffer> Buffer = MakeUnique<FVertexBuffer>(Device.Get(), Desc, Stride);

	return Buffer;
}

TUniquePtr<FIndexBuffer> FRenderDevice::CreateDynamicIndexBuffer(uint32 MaxIndexCount)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.ByteWidth = sizeof(uint32) * MaxIndexCount;
	Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	TUniquePtr<FIndexBuffer> Buffer = MakeUnique<FIndexBuffer>(Device.Get(), Desc, MaxIndexCount);

	return Buffer;
}

TUniquePtr<FConstantBuffer> FRenderDevice::CreateConstantBuffer(uint32 BufferSize)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.ByteWidth = BufferSize;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	TUniquePtr<FConstantBuffer> Buffer = MakeUnique<FConstantBuffer>(Device.Get(), Desc);

	return Buffer;
}

TUniquePtr<FTexture2D> FRenderDevice::CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const void* InitialData)
{
	TUniquePtr<FTexture2D> Texture = MakeUnique<FTexture2D>(Device.Get(), Desc, InitialData);

	return Texture;
}

TUniquePtr<FShader> FRenderDevice::CreateShader(const wchar_t* FileName, D3D11_INPUT_ELEMENT_DESC* InLayoutDesc, size_t InLayoutSize)
{
	TUniquePtr<FShader> Shader = MakeUnique<FShader>();

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
		return nullptr; 
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



void FRenderDevice::Shutdown()
{
	DeviceContext->ClearState();
	DeviceContext->Flush();
}

// RenderDevice.cpp
void FRenderDevice::CreateStates()
{
	// ---------- Rasterizer ----------
	{
		D3D11_RASTERIZER_DESC Desc = {};
		Desc.FillMode = D3D11_FILL_SOLID;
		Desc.CullMode = D3D11_CULL_BACK;
		Desc.FrontCounterClockwise = FALSE;
		Desc.DepthClipEnable = TRUE;

		Device->CreateRasterizerState(&Desc, RasterizerStates[(uint8)ERasterizerState::SolidBack].GetAddressOf());

		Desc.CullMode = D3D11_CULL_NONE;
		Device->CreateRasterizerState(&Desc, RasterizerStates[(uint8)ERasterizerState::SolidNone].GetAddressOf());

		Desc.CullMode = D3D11_CULL_FRONT;
		Device->CreateRasterizerState(&Desc, RasterizerStates[(uint8)ERasterizerState::SolidFront].GetAddressOf());

		Desc.FillMode = D3D11_FILL_WIREFRAME;
		Desc.CullMode = D3D11_CULL_NONE;
		Device->CreateRasterizerState(&Desc, RasterizerStates[(uint8)ERasterizerState::Wireframe].GetAddressOf());
	}

	// ---------- DepthStencil ----------
	{
		D3D11_DEPTH_STENCIL_DESC Desc = {};
		Desc.DepthEnable = TRUE;
		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		Desc.DepthFunc = D3D11_COMPARISON_LESS;
		Desc.StencilEnable = FALSE;

		Device->CreateDepthStencilState(&Desc, DepthStencilStates[(uint8)EDepthStencilState::Default].GetAddressOf());

		Desc.DepthEnable = FALSE;
		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		Device->CreateDepthStencilState(&Desc, DepthStencilStates[(uint8)EDepthStencilState::Disabled].GetAddressOf());
	}

	// ---------- Blend ----------
	{
		D3D11_BLEND_DESC Desc = {};
		Desc.RenderTarget[0].BlendEnable = FALSE;
		Desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		Desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
		Desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		Desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		Desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		Device->CreateBlendState(&Desc, BlendStates[(uint8)EBlendState::Opaque].GetAddressOf());

		Desc.RenderTarget[0].BlendEnable = TRUE;
		Desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		Desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;

		Device->CreateBlendState(&Desc, BlendStates[(uint8)EBlendState::AlphaBlend].GetAddressOf());
	}

	// ---------- Sampler ----------
	{
		D3D11_SAMPLER_DESC Desc = {};
		Desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		Desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		Desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		Desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		Desc.MaxAnisotropy = 1;
		Desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		Desc.MinLOD = 0.0f;
		Desc.MaxLOD = D3D11_FLOAT32_MAX;

		Device->CreateSamplerState(&Desc, SamplerStates[(uint8)ESamplerState::LinearClamp].GetAddressOf());
	}
}

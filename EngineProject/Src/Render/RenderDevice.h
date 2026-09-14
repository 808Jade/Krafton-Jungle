#pragma once

class RenderDevice
{
public:
	RenderDevice();
	~RenderDevice();
private:
	ComPtr<ID3D11RenderDevice> ;
};
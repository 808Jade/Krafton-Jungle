#pragma once

class FRenderDevice
{
public:
	FRenderDevice();
	~FRenderDevice();

	

	ID3D11Device* GetRawPtr() const { return Device.Get(); }
private:
	ComPtr<ID3D11Device> Device;
};
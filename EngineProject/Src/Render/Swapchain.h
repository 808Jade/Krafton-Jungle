#pragma once
class FSwapchain
{
public:
	FSwapchain();
	~FSwapchain();

private:
	ComPtr<IDXGISwapChain> Swapchain;
};
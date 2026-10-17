#pragma once

#include "Core/Types.h"
#include "Containers/Array.h"
#include "Render/RenderCommand.h"
#include "Render/Texture2D.h"

// Forward와 Deferred가 공유하는 뷰포트의 색상, GBuffer, 깊이 리소스를 관리한다.
struct FSceneViewTargets
{
	TUniquePtr<FTexture2D> SceneColor;
	TUniquePtr<FTexture2D> Depth;
	static constexpr uint32 ShadowAtlasSize = 4096;
	TArray<TUniquePtr<FTexture2D>> ShadowAtlases;

	// 추가되는 atlas는 이전 atlas의 가로/세로 두 배 크기를 사용한다.
	static uint32 GetShadowAtlasSize(uint32 AtlasIndex)
	{
		uint32 Size = ShadowAtlasSize;
		for (uint32 Index = 0; Index < AtlasIndex; ++Index)
		{
			if (Size > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION / 2) return 0;
			Size *= 2;
		}
		return Size;
	}

	// 이미 확보한 atlas는 유지하고 필요한 개수까지 추가한다.
	bool InitShadowAtlases(uint32 RequiredCount)
	{
		if (RequiredCount && !GetShadowAtlasSize(RequiredCount - 1)) return false;
		if (static_cast<uint32>(ShadowAtlases.Num()) >= RequiredCount)
			return true;

		D3D11_TEXTURE2D_DESC Desc{};
		Desc.MipLevels = 1;
		Desc.ArraySize = 1;
		Desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		Desc.SampleDesc.Count = 1;
		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		while (static_cast<uint32>(ShadowAtlases.Num()) < RequiredCount)
		{
			Desc.Width = GetShadowAtlasSize(static_cast<uint32>(ShadowAtlases.Num()));
			Desc.Height = Desc.Width;
			auto Atlas = RenderCommand::CreateTexture2D(Desc);
			if (!Atlas || !Atlas->GetRawPtr() || !Atlas->GetDSV() || !Atlas->GetSRV())
				return false;
			ShadowAtlases.Add(std::move(Atlas));
		}
		return true;
	}

	// FXAA처리가 된 SceneColor로 swap하기 위해 사용된다.
	FTexture2D* SwapSceneColorAndBind(TUniquePtr<FTexture2D>& Output)
	{
		const bool bCompatible = Output && 
								Output->GetWidth() == SceneColor->GetWidth() && Output->GetHeight() == SceneColor->GetHeight() &&
								Output->GetFormat() == SceneColor->GetFormat();
		if (!bCompatible)
			return nullptr;
		SceneColor.swap(Output);
		RenderCommand::BindRenderPassNoClear(SceneColor.get(), Depth.get(), SceneColor->GetWidth(), SceneColor->GetHeight());
		return SceneColor.get();
	}

	void Reset()
	{
		SceneColor.reset();
		Depth.reset();
		ShadowAtlases.Empty();
	}

	bool IsValidFor(uint32 Width, uint32 Height) const
	{
		const auto HasSize = [Width, Height](const TUniquePtr<FTexture2D>& Target)
		{
			return Target && Target->GetRawPtr() &&
				Target->GetWidth() == Width && Target->GetHeight() == Height;
		};

		return HasSize(SceneColor) && SceneColor->GetRTV() && SceneColor->GetSRV() &&
			HasSize(Depth) && Depth->GetDSV() && Depth->GetSRV();
	}

	bool Resize(uint32 Width, uint32 Height)
	{
		if (IsValidFor(Width, Height))
			return true;

		// 뷰포트 크기가 바뀌어도 확보한 shadow atlas는 유지한다.
		SceneColor.reset();
		Depth.reset();
		if (Width == 0 || Height == 0)
			return false;

		D3D11_TEXTURE2D_DESC Desc{};
		Desc.Width = Width;
		Desc.Height = Height;
		Desc.MipLevels = 1;
		Desc.ArraySize = 1;
		Desc.SampleDesc.Count = 1;
		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		SceneColor = RenderCommand::CreateTexture2D(Desc);

		// DSV로 쓰고 SRV로 읽을 수 있도록 typeless 리소스로 만든다.
		Desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		Depth = RenderCommand::CreateTexture2D(Desc);

		if (!IsValidFor(Width, Height))
		{
			SceneColor.reset();
			Depth.reset();
			return false;
		}
		return true;
	}
};

#pragma once

#include "Component/AmbientLightComponent.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/SpotLightComponent.h"

class FScene;

enum class ELightType : uint8
{
	Ambient, Directional, Point, Spot,
};

//ShadowMap 프리뷰 용 Texture, SRV
struct FShadowPreview
{
	ComPtr<ID3D11Texture2D> ShadowTexture;
	ComPtr<ID3D11ShaderResourceView> ShadowSRV;
	uint32 Resolution = 0;

	void Reset()
	{
		ShadowTexture.Reset();
		ShadowSRV.Reset();		
	}
};

class FLightSceneProxy
{
public:
	FLightSceneProxy(ULightComponentBase* InComponent, ELightType InType) : Component(InComponent), Type(InType) {}
	virtual ~FLightSceneProxy()
	{
		ShadowPreview.Reset();
	}

	virtual void UpdateFromComponent() = 0;

	ULightComponentBase* GetComponent() const { return Component; }
	ELightType GetLightType() const { return Type; }
	FScene* GetScene() const { return Scene; }
	virtual float GetLightBrightness() const = 0;
	bool IsVisible() const { return bVisible; }
	bool IsCastShadows() const { return bCastShadows; }
	virtual bool ShouldCastShadow() const {
		if (!bVisible || !bCastShadows || GetLightBrightness() < KINDA_SMALL_NUMBER) return false;
		return true;
	}

	int32 GetShadowIndex() const { return ShadowIndex; }

	//디테일 창에서 ShadowMap을 프리뷰하도록 요청하게 된다.
	void SetPreviewShadowMapFlag(bool IsShow) { bPreviewShadowMap = IsShow; }	

	// 이번 프레임 그림자가 없으면 표시 안 함
	ID3D11ShaderResourceView* GetShadowPreviewSRV() const
	{
		return ShadowIndex >= 0 ? ShadowPreview.ShadowSRV.Get() : nullptr;
	}

	// Renderer가 채울 때만 사용
	FShadowPreview& GetShadowPreview() { return ShadowPreview; }

protected:
	ULightComponentBase* Component = nullptr;
	bool bVisible = true;
	bool bCastShadows = true;

private:
	friend class FScene;
	FScene* Scene = nullptr;
	ELightType Type;
	bool bQueuedForUpdate = false;

	// 이번 프레임 배정 결과. 매 프레임 Build에서 다시 쓴다
	int32 ShadowIndex = -1;

	//ShadowMap을 디테일 창에 프리뷰 할지
	bool bPreviewShadowMap = false;
	FShadowPreview ShadowPreview;
};

class FAmbientLightSceneProxy final : public FLightSceneProxy
{
public:
	explicit FAmbientLightSceneProxy(UAmbientLightComponent* InComponent) : FLightSceneProxy(InComponent, ELightType::Ambient) {}

	void UpdateFromComponent() override
	{
		const UAmbientLightComponent* Light = static_cast<const UAmbientLightComponent*>(Component);
		Info = Light->GetAmbientLightInfo();
		bVisible = Light->IsLightVisible();
		bCastShadows = Light->IsCastShadows();
	}

	const FAmbientLightInfo& GetInfo() const { return Info; }
	float GetLightBrightness() const override { return Info.Color.Length(); }

private:
	FAmbientLightInfo Info;
};

class FDirectionalLightSceneProxy final : public FLightSceneProxy
{
public:
	explicit FDirectionalLightSceneProxy(UDirectionalLightComponent* InComponent) : FLightSceneProxy(InComponent, ELightType::Directional) {}

	void UpdateFromComponent() override
	{
		const UDirectionalLightComponent* Light = static_cast<const UDirectionalLightComponent*>(Component);
		Info = Light->GetDirectionalLightInfo();
		bVisible = Light->IsLightVisible();
		bCastShadows = Light->IsCastShadows();

		ShadowInfo = Light->GetShadowInfo();
	}

	const FDirectionalLightInfo& GetInfo() const { return Info; }
	const FShadowInfo& GetShadowInfo() const { return ShadowInfo; }
	float GetLightBrightness() const override { return Info.Color.Length(); }

private:
	FDirectionalLightInfo Info;
	FShadowInfo ShadowInfo;
};

class FPointLightSceneProxy final : public FLightSceneProxy
{
public:
	explicit FPointLightSceneProxy(UPointLightComponent* InComponent) : FLightSceneProxy(InComponent, ELightType::Point) {}

	void UpdateFromComponent() override
	{
		const UPointLightComponent* Light = static_cast<const UPointLightComponent*>(Component);
		Info = Light->GetPointLightInfo();
		bVisible = Light->IsLightVisible();
		bCastShadows = Light->IsCastShadows();

		ShadowInfo = Light->GetShadowInfo();
	}

	bool ShouldCastShadow() const override {
		if (!FLightSceneProxy::ShouldCastShadow()) return false;
		if (Info.AttenuationRadius < KINDA_SMALL_NUMBER) return false;

		return true;
	}

	const FPointLightInfo& GetInfo() const { return Info; }
	const FShadowInfo& GetShadowInfo() const { return ShadowInfo; }
	float GetLightBrightness() const override {
		return Info.Color.Length();
	}

private:
	FPointLightInfo Info;
	FShadowInfo ShadowInfo;
};

class FSpotLightSceneProxy final : public FLightSceneProxy
{
public:
	explicit FSpotLightSceneProxy(USpotLightComponent* InComponent) : FLightSceneProxy(InComponent, ELightType::Spot) {}

	void UpdateFromComponent() override
	{
		const USpotLightComponent* Light = static_cast<const USpotLightComponent*>(Component);
		Info = Light->GetSpotLightInfo();
		bVisible = Light->IsLightVisible();
		bCastShadows = Light->IsCastShadows();

		ShadowInfo = Light->GetShadowInfo();
	}

	bool ShouldCastShadow() const override {
		if (!FLightSceneProxy::ShouldCastShadow()) return false;
		if (Info.AttenuationRadius < KINDA_SMALL_NUMBER) return false;

		return true;
	}

	const FSpotLightInfo& GetInfo() const { return Info; }
	const FShadowInfo& GetShadowInfo() const { return ShadowInfo; }
	float GetLightBrightness() const override {
		return Info.Color.Length();
	}

private:
	FSpotLightInfo Info;
	FShadowInfo ShadowInfo;
};

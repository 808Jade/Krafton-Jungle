#pragma once

#include "Component/AmbientLightComponent.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/SpotLightComponent.h"

class FScene;

enum class ELightType : uint8
{
	Ambient, Directional, Point, Spot,
};

class FLightSceneProxy
{
public:
	FLightSceneProxy(ULightComponentBase* InComponent, ELightType InType) : Component(InComponent), Type(InType) {}
	virtual ~FLightSceneProxy() = default;

	virtual void UpdateFromComponent() = 0;

	ULightComponentBase* GetComponent() const { return Component; }
	ELightType GetLightType() const { return Type; }
	FScene* GetScene() const { return Scene; }
	bool IsVisible() const { return bVisible; }

	int32 GetShadowIndex() const { return ShadowIndex; }

protected:
	ULightComponentBase* Component = nullptr;
	bool bVisible = true;

private:
	friend class FScene;
	FScene* Scene = nullptr;
	ELightType Type;
	bool bQueuedForUpdate = false;
	// 이번 프레임 배정 결과. 매 프레임 Build에서 다시 쓴다
	int32 ShadowIndex = -1;       
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
	}

	const FAmbientLightInfo& GetInfo() const { return Info; }

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

		ShadowInfo = Light->GetShadowInfo();
	}

	const FDirectionalLightInfo& GetInfo() const { return Info; }
	const FShadowInfo& GetShadowInfo() const { return ShadowInfo; }

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

		ShadowInfo = Light->GetShadowInfo();
	}

	const FPointLightInfo& GetInfo() const { return Info; }
	const FShadowInfo& GetShadowInfo() const { return ShadowInfo; }

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
		ShadowInfo = Light->GetShadowInfo();
	}

	const FSpotLightInfo& GetInfo() const { return Info; }
	const FShadowInfo& GetShadowInfo() const { return ShadowInfo; }

private:
	FSpotLightInfo Info;
	FShadowInfo ShadowInfo;
};

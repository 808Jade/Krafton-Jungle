#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleLight.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleHelper.h"
#include "Components/ParticleSystemComponent.h"
#include "Components/PointLightComponent.h"
#include "Math/Noise.h"
#include "Serialization/MathSerialization.h"

#include <cmath>

namespace
{
	uint8 ToByte(float V)
	{
		return static_cast<uint8>(std::clamp(V, 0.0f, 1.0f) * 255.0f + 0.5f);
	}
}

void UParticleModuleLight::Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle)
{
	float& Lit = *reinterpret_cast<float*>(reinterpret_cast<uint8*>(&Particle) + PayloadOffset);
	Lit = (SpawnFraction >= 1.0f || FMath::FRand() < SpawnFraction) ? 1.0f : 0.0f;
}

void UParticleModuleLight::Update(FParticleEmitterInstance& Owner, int32 PayloadOffset, float DeltaTime)
{
	UParticleSystemComponent* Component = Owner.Component;
	if (!Component || !Component->GetWorld() || !Component->IsVisible())
	{
		return;
	}

	const bool bLocalSpace = Owner.bUseLocalSpace;
	const FMatrix SimulationToWorld = Owner.SimulationToWorld;
	const FVector ComponentScale = Component->GetWorldScale3D().GetAbs();
	const float MaxComponentScale = std::max({ ComponentScale.X, ComponentScale.Y, ComponentScale.Z });
	const float RadiusCap = std::max(MaxRadius, 0.01f);

	int32 Used = 0;
	for (int32 i = 0; i < Owner.ActiveParticles; ++i)
	{
		if (MaxLights > 0 && Used >= MaxLights)
		{
			break;
		}

		DECLARE_PARTICLE_PTR(Particle, Owner.ParticleData + Owner.ParticleStride * Owner.ParticleIndices[i]);
		const float Lit = *reinterpret_cast<const float*>(reinterpret_cast<const uint8*>(Particle) + PayloadOffset);
		if (Lit < 0.5f)
		{
			continue;
		}

		// 세기 = 기준값 x (수명 커브) x (알파). 거의 0이면 라이트를 켜지 않는다.
		float LightIntensity = Intensity;
		if (bUseBrightnessOverLife)
		{
			LightIntensity *= BrightnessOverLife.Eval(std::clamp(Particle->RelativeTime, 0.0f, 1.0f), 1.0f);
		}
		if (bScaleByParticleAlpha)
		{
			LightIntensity *= std::clamp(Particle->Color.A, 0.0f, 1.0f);
		}
		if (LightIntensity <= 0.001f)
		{
			continue;
		}

		// 색은 최대 1로 맞추고 넘친 만큼(HDR)은 세기로 옮긴다. 라이트 색은 8비트라서 1을 넘길 수 없다.
		FVector Color(TintColor.R, TintColor.G, TintColor.B);
		if (bUseParticleColor)
		{
			Color = FVector(Particle->Color.R * TintColor.R, Particle->Color.G * TintColor.G, Particle->Color.B * TintColor.B);
		}
		const float Peak = std::max({ Color.X, Color.Y, Color.Z });
		if (Peak <= 0.001f)
		{
			continue;
		}
		if (Peak > 1.0f)
		{
			Color = Color * (1.0f / Peak);
			LightIntensity *= Peak;
		}

		const float SizeLength = std::max(std::abs(Particle->Size.X), std::abs(Particle->Size.Y));
		const float Radius = std::min(RadiusScale * SizeLength * MaxComponentScale, RadiusCap);
		if (Radius < 0.02f)
		{
			continue;
		}

		UPointLightComponent* Light = Component->AcquireParticleLight();
		if (!Light)
		{
			break; // 컴포넌트의 라이트 풀이 가득 찼다.
		}

		const FVector WorldLocation = bLocalSpace ? SimulationToWorld.TransformPosition(Particle->Location) : Particle->Location;
		Light->SetWorldLocation(WorldLocation, false);
		Light->SetLightColor(FColor(ToByte(Color.X), ToByte(Color.Y), ToByte(Color.Z), 255));
		Light->SetIntensity(LightIntensity);
		Light->SetAttenuationRadius(Radius);
		Light->SetFalloff(std::max(FalloffExponent, 0.0f));
		// 풀의 라이트는 프레임마다 다른 모듈이 빌려 갈 수 있으니 그림자 설정도 빌릴 때마다 이 모듈의 값으로 맞춘다.
		const bool bShadowNow = bCastShadows && LightIntensity >= ShadowMinIntensity;
		Light->SetCastShadows(bShadowNow);
		if (bShadowNow)
		{
			Light->SetShadowMapResolution(ShadowMapResolution);
		}
		++Used;
	}
}

void UParticleModuleLight::GetCurves(TArray<FParticleCurveRef>& OutCurves)
{
	OutCurves.Add({ "Brightness Over Life", &BrightnessOverLife, { 1.0f, 0.85f, 0.3f, 1.0f }, -1 });
}

void UParticleModuleLight::Serialize(FStructuredArchive::FRecord Record)
{
	// ObjectLabel + 리플렉션 프로퍼티. 커브는 TArray라 PROPERTY로 등록할 수 없어서 여기서 직접 저장한다.
	Super::Serialize(Record);

	if (TOptional<FStructuredArchive::FSlot> Slot = Record.TryEnterField("BrightnessOverLife", true); Slot.IsSet())
	{
		BrightnessOverLife.Serialize(Slot.GetValue());
	}
}

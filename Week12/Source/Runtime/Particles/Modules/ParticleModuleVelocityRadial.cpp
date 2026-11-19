#include "EnginePCH.h"
#include "Particles/Modules/ParticleModuleVelocityRadial.h"
#include "Math/Noise.h"

void UParticleModuleVelocityRadial::Spawn(FParticleEmitterInstance& Owner, int32 PayloadOffset, float SpawnTime, FBaseParticle& Particle)
{
	// 구 표면의 균일 분포: z를 [-1, 1]에서 고르게 뽑고 각도는 [0, 2pi). (구면 좌표를 그냥 뽑으면 극쪽에 몰린다)
	constexpr float TwoPi = 6.28318530718f;
	const float Z = FMath::FRandRange(-1.0f, 1.0f);
	const float Phi = FMath::FRandRange(0.0f, TwoPi);
	const float Radius = std::sqrt(std::max(0.0f, 1.0f - Z * Z));

	const FVector Direction(Radius * std::cos(Phi) * AxisScale.X, Radius * std::sin(Phi) * AxisScale.Y, Z * AxisScale.Z);
	const FVector V = Direction * Speed.Sample();

	Particle.Velocity += V;
	Particle.BaseVelocity += V;
}

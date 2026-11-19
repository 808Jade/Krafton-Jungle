#pragma once
#include "ActorComponent.h"

#include <sol/sol.hpp>

class FLuaVM;
struct FHitResult;
struct FParticleCollisionEvent;

class UScriptComponent : public UActorComponent
{
	DECLARE_CLASS(UScriptComponent, UActorComponent)

	REFLECT_START(ClassName)
		PROPERTY_FLAGS(ScriptPath, CPF_None)
		REFLECT_END()

public:
	virtual ~UScriptComponent() override;

	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;
	virtual void EndPlay() override;

	using Super::Serialize;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;

	void OnOverlap(AActor* OtherActor);
	void OnEndOverlap(AActor* OtherActor);
	void OnHit(AActor* OtherActor, const FHitResult& Hit);
	// 같은 액터의 파티클 시스템 컴포넌트에서 파티클이 충돌하면 호출된다. (Collision 모듈의 bGenerateEvents가 켜져 있을 때)
	void OnParticleCollide(const FParticleCollisionEvent& Event);
	void OnAnimNotify(const FString& NotifyName);
	void OnAnimNotifyBegin(const FString& NotifyName);
	void OnAnimNotifyEnd(const FString& NotifyName);
	void OnAnimNotifyTick(const FString& NotifyName, float DeltaSeconds);

	bool CreateScriptFromTemplate(const std::filesystem::path& NewScriptPath);
	bool LoadScript();
	bool EnsureScriptFile();
	bool SetScriptPath(const FString& NewFileName);

	void BindScriptContext(sol::environment& TargetEnvironment);

	const FString& GetScriptPath() const { return ScriptPath; }

private:
	sol::environment Environment;

	sol::protected_function BeginPlayFunction;
	sol::protected_function TickFunction;
	sol::protected_function EndPlayFunction;
	sol::protected_function OverlapFunction;
	sol::protected_function EndOverlapFunction;
	sol::protected_function HitFunction;
	sol::protected_function ParticleCollideFunction;
	sol::protected_function AnimNotifyFunction;
	sol::protected_function AnimNotifyBeginFunction;
	sol::protected_function AnimNotifyEndFunction;
	sol::protected_function AnimNotifyTickFunction;

	FLuaVM* LuaVM = nullptr;
	sol::state* State = nullptr;

	struct FCollisionBinding
	{
		uint32 ShapeUUID;
		uint32 BeginOverlapHandle;
		uint32 EndOverlapHandle;
		uint32 HitHandle;
	};
	TArray<FCollisionBinding> CollisionBindings;

	// 파티클 시스템 컴포넌트의 OnParticleCollide 구독 기록 (해제할 때 쓴다)
	struct FParticleCollisionBinding
	{
		uint32 ComponentUUID;
		uint32 Handle;
	};
	TArray<FParticleCollisionBinding> ParticleCollisionBindings;
	// Broadcast의 복사본에 남은 콜백도 EndPlay/소멸 후에는 실행하지 않는다.
	TSharedPtr<bool> CollisionCallbackLifetime;

	void BindCollisionEvents();
	void UnbindCollisionEvents();

	FString ScriptPath;

	std::filesystem::file_time_type LastWriteTime{};
	float HotReloadCheckTimer = 0.0f;

	bool ReloadScript();
	void CheckHotReload(float DeltaTime);
};

function BeginPlay()
print("BeginPlay!\n")
end

function EndPlay()
print("EndPlay!\n")
end

function OnOverlap(OtherActor)
print("OnOverlap!\n")
end

-- Shape의 BeginOverlap은 기존 OnOverlap으로 전달됩니다.
function OnEndOverlap(OtherActor)
print("OnEndOverlap!\n")
end

-- Hit는 Sweep 이동으로 발생하며 Hit 정보는 Lua에 복사되어 전달됩니다.
function OnHit(OtherActor, Hit)
print("OnHit!", Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z)
end

-- 같은 액터의 파티클 시스템에서 파티클이 다른 컴포넌트와 충돌하면 호출됩니다.
-- 파티클 에디터에서 Collision 모듈의 bGenerateEvents를 켜야 합니다. 위치와 방향은 월드 좌표입니다.
-- Event: EventName, EmitterTime, ParticleRelativeTime(0~1), CollisionCount(1부터),
--        Location(표면 위 충돌점), Velocity(튕기기 전 속도), Normal, HitActor
-- function OnParticleCollide(Event)
--     print("OnParticleCollide!", Event.EventName, Event.Location.X, Event.Location.Y, Event.Location.Z)
-- end

function Tick(dt)
print("Tick!\n")
end

function OnReload()
print("Reload!\n")
end

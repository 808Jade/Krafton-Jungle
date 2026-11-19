-- 번개 한 자리(LightningStrike.particle을 쓰는 파티클 액터)에 붙이는 천둥 소리 스크립트.
-- 번개를 치고 싶은 곳마다 이 액터를 복사해서 놓으면 된다. (파티클과 이 스크립트가 같은 액터에 있어야 이벤트가 들어온다)
--
-- LightningStrike.particle의 StrikeTimer가 번개를 칠 때 'ThunderStrike' 이벤트를 보내고, 그것이 OnParticleCollide로 들어온다.
-- 천둥은 번쩍인 뒤 조금 있다가, 플레이어에게서 멀수록 더 늦고 작게 들린다.

local ThunderSound = "Assets/Sounds/Thunder.mp3"

local VolumeNear = 1.0          -- 가까운 번개
local VolumeFar = 0.35          -- FarDistance보다 먼 번개
local FarDistance = 80.0
local BaseDelay = 0.25          -- 번쩍임과 천둥 사이의 기본 간격(초)
local SpeedOfSound = 343.0      -- m/s. 거리만큼 더 늦게 들린다.
local MaxExtraDelay = 0.4
local FadeOutSeconds = 1.0      -- 이 자리에서 다음 번개가 치면 앞 천둥을 이만큼 동안 줄인다. (Thunder.mp3가 11초쯤이라 겹치지 않게)

local Clock = 0.0
local Pending = {}              -- { Time, Volume }
local CurrentHandle = 0

local function ListenerLocation()
    local manager = GetPlayerCameraManager()
    local target = manager and manager:GetViewTarget() or nil
    if target ~= nil then
        return target:GetActorLocation()
    end
    return nil
end

function BeginPlay()
end

function EndPlay()
    if CurrentHandle ~= 0 then
        StopSound(CurrentHandle)
        CurrentHandle = 0
    end
    Pending = {}
end

function OnOverlap(OtherActor)
end

function OnEndOverlap(OtherActor)
end

function OnHit(OtherActor, Hit)
end

-- 이름은 충돌 콜백이지만 파티클 Event Generator의 이벤트도 여기로 들어온다.
function OnParticleCollide(Event)
    if Event.EventName ~= "ThunderStrike" then
        return
    end

    local volume = VolumeNear
    local delay = BaseDelay
    local listener = ListenerLocation()
    if listener ~= nil then
        local dx = Event.Location.X - listener.X
        local dy = Event.Location.Y - listener.Y
        local dz = Event.Location.Z - listener.Z
        local distance = math.sqrt(dx * dx + dy * dy + dz * dz)
        local t = math.min(distance / FarDistance, 1.0)
        volume = VolumeNear + (VolumeFar - VolumeNear) * t
        delay = delay + math.min(distance / SpeedOfSound, MaxExtraDelay)
    end
    table.insert(Pending, { Time = Clock + delay, Volume = volume })
end

function Tick(dt)
    Clock = Clock + dt
    for i = #Pending, 1, -1 do
        local thunder = Pending[i]
        if Clock >= thunder.Time then
            if CurrentHandle ~= 0 and IsSoundPlaying(CurrentHandle) then
                FadeSound(CurrentHandle, 0.0, FadeOutSeconds)
            end
            CurrentHandle = PlaySound(ThunderSound, false, thunder.Volume)
            if CurrentHandle ~= 0 then
                -- 같은 소리가 반복돼도 덜 단조롭게 높이를 조금씩 바꾼다.
                SetSoundPitch(CurrentHandle, 0.85 + math.random() * 0.25)
            end
            table.remove(Pending, i)
        end
    end
end

function OnReload()
end

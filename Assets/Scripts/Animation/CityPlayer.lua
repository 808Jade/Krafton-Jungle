-- 3.AbandonedCityStorm 씬의 플레이어. AnimationTest.lua(2.animationTest 씬)를 그대로 가져와서
--   1) 화면에 애니메이션 상태(Steps, 노티파이 이름, HIT WINDOW)를 띄우던 UI를 뺐고
--   2) 마우스 우클릭을 누른 채로 끌면 화면을 둘러볼 수 있게 했다. (원래 우클릭이던 AttackReady 자세는 쓰지 않는다)
-- 이동·점프·공격(좌클릭)·피격(H)·사망(Y)·구르기(R)와 애니메이션 파라미터는 AnimationTest.lua와 같다.

local State = { HitWindow = false, ChargeHandle = 0 }

local LandShake = {
    Type = "Perlin", Duration = 0.4, BlendInTime = 0.02, BlendOutTime = 0.3,
    Pitch = { Amplitude = 3.0, Frequency = 20.0 }, Roll = { Amplitude = 2.0, Frequency = 18.0 },
}

-- 블렌드 스페이스(BS_LocomotionCity)는 재생 속도를 "이동 속도 ÷ 클립의 원래 속도"로 맞춘다. (Walk_Loop 0.975, Jog_Fwd_Loop 5.357 m/s)
-- 샘플 위치: Walk 1.5, Jog 5.357. 걷기(1.5)는 Walk만 1.54배로, 달리기(6.0)는 Jog만 1.12배로 재생해서 디딘 발이 거의 멈춰 있다.
-- 두 값 사이(가속·감속하는 순간)는 Walk와 Jog를 섞는데, 보폭 리듬이 달라 약간 미끄러진다.
-- 속도를 바꾸면 BS_LocomotionCity.blendspace의 Walk 샘플 X도 walkSpeed와 같게 맞춘다.
local walkSpeed = 1.5
local runSpeed = 6.0
local jumpSpeed = 10.0

-- 둘러보기: 좌우는 액터를 돌리고(카메라가 캡슐의 자식이라 같이 돈다), 위아래는 카메라를 피벗 둘레로 돌린다.
-- 시작 값은 씬의 카메라 배치(캡슐 기준 (-3, 0, 1.5), Pitch 24.2)와 같은 자리가 되도록 맞췄다.
local CameraDistance = 3.29
local CameraPivotHeight = 0.15
local CameraPitch = 24.2
local CameraPitchMin, CameraPitchMax = -5.0, 60.0
local LookSensitivityYaw = 0.15
local LookSensitivityPitch = 0.12

-- 비 소리: 비 파티클이 플레이어를 따라다니니 소리도 플레이어가 낸다. 배경 음악처럼 계속 반복하고, 처음 2초 동안 서서히 커진다.
-- (천둥 소리는 번개 액터마다 붙은 ThunderStrike.lua가 낸다)
local RainSound = "Assets/Sounds/Rain.mp3"
local RainVolume = 0.6
local RainFadeInSeconds = 2.0
local RainHandle = 0

local function Clamp(value, low, high)
    return math.max(low, math.min(value, high))
end

local function WrapYaw(yaw)
    return (yaw + 180.0) % 360.0 - 180.0
end

local function ApplyCamera(camera)
    if camera == nil then
        return
    end
    local pitch = math.rad(CameraPitch)
    camera:SetRelativeLocation(FVector.new(
        -CameraDistance * math.cos(pitch),
        0.0,
        CameraPivotHeight + CameraDistance * math.sin(pitch)))
    camera:SetRelativeRotation(FRotator.new(CameraPitch, 0.0, 0.0))
end

function BeginPlay()
    local camera = self:GetCamera()
    local manager = GetPlayerCameraManager()

    if camera ~= nil and manager ~= nil then
        assert(manager:SetViewTargetCamera(camera))
    end
    ApplyCamera(camera)

    if RainHandle == 0 or not IsSoundPlaying(RainHandle) then
        RainHandle = PlaySound(RainSound, true, 0.0)
        if RainHandle ~= 0 then
            FadeSound(RainHandle, RainVolume, RainFadeInSeconds)
        end
    end

    local movement = self:GetMovementComponent()

    if movement == nil then
        return
    end

    movement:SetMaxSpeed(walkSpeed)
    movement:SetAcceleration(20.0)
    movement:SetBrakingDeceleration(30.0)
    movement:SetGravityAcceleration(20.0)
    movement:SetGravityScale(1.0)
    -- 바닥 메시(씬에서 bBlockComponent를 켠 Plane.002)의 실제 표면을 따라 걷는다. 움푹 팬 곳도 내려갔다 올라온다.
    movement:SetWalkOnBlockingMeshes(true)
end

function EndPlay()
    if RainHandle ~= 0 then
        StopSound(RainHandle)
        RainHandle = 0
    end
end

function OnOverlap(OtherActor)
end

function OnEndOverlap(OtherActor)
end

function OnHit(OtherActor, Hit)
end

-- 우클릭을 누르고 있는 동안 마우스 이동량으로 시점을 돌린다.
local function UpdateLook(mesh)
    if not IsMouseDown(MouseButton.Right) then
        return
    end
    local dx = GetMouseDeltaX()
    local dy = GetMouseDeltaY()
    if dx == 0 and dy == 0 then
        return
    end

    if dx ~= 0 then
        local turn = dx * LookSensitivityYaw
        local rotation = self:GetActorRotation()
        rotation.Yaw = WrapYaw(rotation.Yaw + turn)
        self:SetActorRotation(rotation)
        -- 메시는 액터의 자식이라 같이 돌아 버린다. 서 있을 때 캐릭터가 제자리에서 돌지 않도록 그만큼 되돌린다.
        -- (움직이는 중에는 아래에서 진행 방향으로 다시 맞춘다)
        if mesh ~= nil then
            local meshRotation = mesh:GetRelativeRotation()
            meshRotation.Yaw = WrapYaw(meshRotation.Yaw - turn)
            mesh:SetRelativeRotation(meshRotation)
        end
    end

    if dy ~= 0 then
        CameraPitch = Clamp(CameraPitch + dy * LookSensitivityPitch, CameraPitchMin, CameraPitchMax)
        ApplyCamera(self:GetCamera())
    end
end

function Tick(dt)
    local movement = self:GetMovementComponent()

    if movement == nil then
        return
    end

    local x = 0.0
    local y = 0.0

    local mesh = self:FindSceneComponentByName("SkeletalMeshComponent")
    if mesh == nil then
        return
    end

    UpdateLook(mesh)

    if IsKeyDown(Key.W) then x = x + 1.0 end
    if IsKeyDown(Key.S) then x = x - 1.0 end
    if IsKeyDown(Key.D) then y = y + 1.0 end
    if IsKeyDown(Key.A) then y = y - 1.0 end

    -- Velocity is in world space; mesh rotation is relative to the actor.
    local velocity = movement:GetVelocity()
    local horizontalSpeedSquared = velocity.X * velocity.X + velocity.Y * velocity.Y
    if horizontalSpeedSquared > 0.0001 then
        local movementYaw = math.deg(math.atan2(velocity.Y, velocity.X))
        local actorYaw = self:GetActorRotation().Yaw
        local rotation = mesh:GetRelativeRotation()
        -- This mesh faces -X at zero relative yaw.
        rotation.Yaw = (movementYaw - actorYaw + 180.0 + 180.0) % 360.0 - 180.0
        mesh:SetRelativeRotation(rotation)
    end

    local yaw = math.rad(self:GetActorRotation().Yaw)
    local cosYaw = math.cos(yaw)
    local sinYaw = math.sin(yaw)

    local worldDirection = FVector.new(
        x * cosYaw - y * sinYaw,
        x * sinYaw + y * cosYaw,
        0.0
    )

    movement:SetMaxSpeed(IsKeyDown(Key.Shift) and runSpeed or walkSpeed)

    -- 입력이 없을 때도 (0, 0, 0)을 전달한다. 대각선 정규화는 C++ SetMoveInput에서 처리한다.
    movement:SetMoveInput(worldDirection)

    if IsKeyPressed(Key.Space) and movement:IsGrounded() then
        movement:SetVerticalVelocity(jumpSpeed)
    end

    -- 메시 교체 등으로 AnimInstance가 재생성될 수 있으므로 사용할 때 조회한다.
    local anim = self:GetAnimInstance()

    if anim == nil or not anim:IsGraphReady() then
        return
    end

    if IsMousePressed(MouseButton.Left) then
        assert(anim:SetTrigger("AttackRequested"), anim:GetLastError())
    end
    if IsKeyPressed(Key.H) then
        assert(anim:SetTrigger("HitRequested"), anim:GetLastError())
    end
    if IsKeyPressed(Key.Y) then
        assert(anim:SetTrigger("DeathRequested"), anim:GetLastError())
    end
    if IsKeyPressed(Key.R) then
        assert(anim:SetTrigger("RollRequested"), anim:GetLastError())
    end

    anim:SetFloat("Speed", movement:GetGroundSpeed())
    anim:SetBool("IsGrounded", movement:IsGrounded())
    anim:SetFloat("AttackReady", 0.0)
end

function OnReload()
end

function OnAnimNotify(Name)
    if Name == "LeftFoot" then
        PlaySound("Assets/Sounds/FootstepLeft.wav", false, 0.8)
    elseif Name == "RightFoot" then
        PlaySound("Assets/Sounds/FootstepRight.wav", false, 0.8)
    elseif Name == "Land" then
        PlaySound("Assets/Sounds/JumpLand.wav", false, 1.0)
        local Manager = GetPlayerCameraManager()
        if Manager then
            Manager:AddCameraModifier("UCameraModifier_CameraShake", { Pattern = LandShake, Scale = 1.0 })
        end
    end
end

function OnAnimNotifyBegin(Name)
    if Name == "SlowMo" then
        SetGlobalTimeDilation(0.3)
    elseif Name == "HitWindow" then
        State.HitWindow = true
        SetGlobalTimeDilation(0.3)
        PlaySound("Assets/Sounds/SwordSwing.wav", false, 0.9)
    elseif Name == "Charge" then
        State.ChargeHandle = PlaySound("Assets/Sounds/심장소리.mp3", true, 0.7)
    end
end

function OnAnimNotifyEnd(Name)
    if Name == "SlowMo" then
        SetGlobalTimeDilation(1.0)
    elseif Name == "HitWindow" then
        State.HitWindow = false
        SetGlobalTimeDilation(1.0)
    elseif Name == "Charge" and State.ChargeHandle ~= 0 then
        StopSound(State.ChargeHandle)
        State.ChargeHandle = 0
    end
end

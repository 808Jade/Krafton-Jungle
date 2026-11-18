-- Attach to an actor with a ParticleSystemComponent and a Beam emitter at index 0.
-- Use Assets/Particles/BeamParticle.particle (Source/Target Method = UserSet).
-- Disable LockOnSpawn on both Source and Target modules for continuous movement.
-- With Absolute disabled, all points are component-local.
-- SetBeam*Point only stores a value; it does not change the module Method.
local Particles = nil
local Elapsed = 0
local EmitterIndex = 0

function BeginPlay()
    Elapsed = 0
    Particles = self:GetParticleSystemComponent()
    -- For actors with multiple particle components, use:
    -- Particles = self:FindParticleSystemComponentByName("ParticleSystemComponent")
    if Particles == nil then
        print("BeamPointControl: particle component not found")
        return
    end
    if not Particles:SetBeamSourcePoint(EmitterIndex, FVector.new(0, 0, 0)) then
        print("BeamPointControl: emitter 0 must be a Beam emitter")
        Particles = nil
    end
end

function Tick(dt)
    if Particles == nil then return end
    Elapsed = Elapsed + dt
    Particles:SetBeamSourcePoint(EmitterIndex, FVector.new(0, 0, math.sin(Elapsed)))
    Particles:SetBeamTargetPoint(EmitterIndex, FVector.new(5, math.sin(Elapsed) * 2, 2))
end

function EndPlay()
    if Particles == nil then return end
    Particles:ClearBeamSourcePoint(EmitterIndex)
    Particles:ClearBeamTargetPoint(EmitterIndex)
    Particles = nil
end

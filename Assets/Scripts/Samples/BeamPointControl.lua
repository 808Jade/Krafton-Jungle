-- Attach to an actor with a ParticleSystemComponent and a Beam emitter at index 0.
-- Disable LockOnSpawn on both Source and Target modules for continuous movement.
-- All points are component-local. Overrides affect every beam in this emitter.
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

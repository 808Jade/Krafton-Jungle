-- Source module: Method = Actor, ParameterName = BeamOrigin, LockOnSpawn = false.
-- Target module: Method = Component, ParameterName = BeamEnd, LockOnSpawn = false.
-- Add a SceneComponent named BeamEndPoint to this actor and position it as desired.
-- Actor/Component endpoints always follow world positions, regardless of Absolute.
local Particles = nil

function BeginPlay()
    Particles = self:GetParticleSystemComponent()
    local Endpoint = self:FindSceneComponentByName("BeamEndPoint")
    if Particles == nil or Endpoint == nil then
        print("BeamReferenceControl: ParticleSystemComponent or BeamEndPoint not found")
        return
    end
    -- Each name is forwarded to every beam instance in this particle component.
    Particles:SetBeamActorParameter("BeamOrigin", self)
    Particles:SetBeamComponentParameter("BeamEnd", Endpoint)
end

function EndPlay()
    if Particles == nil then return end
    -- Clearing a reference restores the module's fallback distribution.
    Particles:ClearBeamParameter("BeamOrigin")
    Particles:ClearBeamParameter("BeamEnd")
    Particles = nil
end

local Shotgun        = ...  -- Retrieve the ComponentCarriedWeaponT instance that is responsible for this script.
local Entity         = Shotgun:GetEntity()
local HumanPlayer    = Entity:GetComponent("HumanPlayer")
local PlayerScript   = Entity:GetComponent("Script")
local Inventory      = Entity:GetComponent("Inventory")
local Model1stPerson = nil


-- Symbolic names for the animation sequences of the 1st-person weapon model.
local ANIM_IDLE         = 0
local ANIM_SHOOT1       = 1
local ANIM_SHOOT2       = 2
local ANIM_INSERT       = 3
local ANIM_END_RELOAD   = 4
local ANIM_BEGIN_RELOAD = 5
local ANIM_DRAW         = 6


local function Update1stPersonModel()
    -- On the client, our entity's *children* may arrive over the network only *after*
    -- this script has initially been run (and this component's OnInit() has been called).
    -- Therefore, we have to defer the Model1stPerson init until it is first used.
    if not Model1stPerson then
        Model1stPerson = Entity:GetChildren()[2]:GetComponent("Model")
    end
end


local function OnSequenceWrap_Sv(Model)     -- Model == Model1stPerson as assigned in Draw() below.
    local self   = Shotgun
    local SequNr = Model:get("Animation")

    if SequNr == ANIM_DRAW then
        Model:set("Animation", ANIM_IDLE)
        return
    end

 -- if SequNr == ANIM_HOLSTER then
 --     HumanPlayer:SelectNextWeapon()
 --     return
 -- end

    if SequNr == ANIM_BEGIN_RELOAD then
        Model:set("Animation", ANIM_INSERT)
        return
    end

    if SequNr == ANIM_INSERT then
        -- assert(self:CanReload())
        Inventory:Add("Shells", -1)
        self:set("PrimaryAmmo", self:get("PrimaryAmmo") + 1)

        if not self:CanReload() then
            Model:set("Animation", ANIM_END_RELOAD)
        end
        return
    end

    if SequNr == ANIM_END_RELOAD then
        Model:set("Animation", ANIM_IDLE)
        return
    end

    if SequNr == ANIM_SHOOT1 or SequNr == ANIM_SHOOT2 then
        if self:get("PrimaryAmmo") < 1 and self:CanReload() then
            Model:set("Animation", ANIM_BEGIN_RELOAD)
        else
            Model:set("Animation", ANIM_IDLE)
        end
        return
    end

 -- if SequNr == ANIM_IDLE then
 --     return
 -- end
end


function Shotgun:IsIdle()
    Update1stPersonModel()

    return Model1stPerson:get("Animation") == ANIM_IDLE
end


function Shotgun:Draw()
    Update1stPersonModel()

    Model1stPerson:set("Show", true)
    Model1stPerson:set("Name", self:get("Model1stPerson"))
    Model1stPerson:set("Animation", ANIM_DRAW)
    Model1stPerson.OnSequenceWrap_Sv = OnSequenceWrap_Sv
end


function Shotgun:Holster()
    -- Unfortunately, the shotgun model does not support holstering (it has no "holster" sequence).
    -- Update1stPersonModel()
    -- Model1stPerson:set("Animation", ANIM_HOLSTER)
    return false
end


function Shotgun:CanReload()
    return self:get("PrimaryAmmo") < self:get("MaxPrimaryAmmo") and Inventory:get("Shells") > 0
end


function Shotgun:FirePrimary()
    if not self:IsIdle() then return end

    if self:get("PrimaryAmmo") < 1 then
        if self:CanReload() then
            Model1stPerson:set("Animation", ANIM_BEGIN_RELOAD)
        end
    else
        self:set("PrimaryAmmo", self:get("PrimaryAmmo") - 1)
        PlayerScript:PostEvent(PlayerScript.EVENT_TYPE_PRIMARY_FIRE)

        Model1stPerson:set("Animation", ANIM_SHOOT1)
    end

    -- TODO: inflict damage
end


function Shotgun:FireSecondary()
    if not self:IsIdle() then return end

    if self:get("PrimaryAmmo") < 2 then
        if self:CanReload() then
            Model1stPerson:set("Animation", ANIM_BEGIN_RELOAD)
        end
    else
        self:set("PrimaryAmmo", self:get("PrimaryAmmo") - 2)
        PlayerScript:PostEvent(PlayerScript.EVENT_TYPE_SECONDARY_FIRE)

        Model1stPerson:set("Animation", ANIM_SHOOT2)
    end

    -- TODO: inflict damage
end


function Shotgun:PickedUp()
    if self:get("IsAvail") then
        -- If we already have the shotgun, and our inventory already has the maximum number of shells,
        -- don't pick up anything.
        if Inventory:CheckMax("Shells") then
            return false
        end

        Inventory:Add("Shells", 16)
    else
        self:set("IsAvail", true)
        self:set("PrimaryAmmo", self:get("MaxPrimaryAmmo"))

        Inventory:Add("Shells", 8)

        HumanPlayer:SelectWeapon(self)
    end

    -- Console.Print("primary Ammo: " .. self:get("PrimaryAmmo") .. "\n")
    -- Console.Print("inv shells:   " .. Inventory:get("Shells") .. "\n")
    return true
end


function Shotgun:GetCrosshairInfo()
    return "Gui/CrossHair2", true
end


function Shotgun:GetAmmoString()
    return string.format("Ammo %2u (%2u)", self:get("PrimaryAmmo"), Inventory:get("Shells"))
end


function Shotgun:ProcessEvent(EventType, NumEvents)
    Console.Print("Shotgun:ProcessEvent(" .. tostring(EventType) .. ", " .. tostring(NumEvents) .. ")\n")
end

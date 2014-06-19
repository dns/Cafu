/*
=================================================================================
This file is part of Cafu, the open-source game engine and graphics engine
for multiplayer, cross-platform, real-time 3D action.
Copyright (C) 2002-2013 Carsten Fuchs Software.

Cafu is free software: you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

Cafu is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Cafu. If not, see <http://www.gnu.org/licenses/>.

For support and more information about Cafu, visit us at <http://www.cafu.de>.
=================================================================================
*/

#include "CompHumanPlayer.hpp"
#include "AllComponents.hpp"
#include "CompCollisionModel.hpp"
#include "CompModel.hpp"                // for implementing CheckGUIs()
#include "CompPhysics.hpp"
#include "CompPlayerPhysics.hpp"
#include "CompScript.hpp"
#include "Entity.hpp"
#include "EntityCreateParams.hpp"
#include "World.hpp"

#include "HumanPlayer/CompCarriedWeapon.hpp"
#include "HumanPlayer/Constants_AmmoSlots.hpp"
#include "HumanPlayer/Constants_WeaponSlots.hpp"
#include "HumanPlayer/cw_357.hpp"

#include "ClipSys/ClipWorld.hpp"
#include "ClipSys/TraceResult.hpp"
#include "GuiSys/GuiImpl.hpp"
#include "MaterialSystem/Material.hpp"
#include "MaterialSystem/Renderer.hpp"
#include "Math3D/Angles.hpp"
#include "Math3D/Matrix3x3.hpp"
#include "Models/AnimPose.hpp"          // for implementing CheckGUIs()
#include "Models/Model_cmdl.hpp"
#include "OpenGL/OpenGLWindow.hpp"      // for implementing CheckGUIs()
#include "ParticleEngine/ParticleEngineMS.hpp"
#include "String.hpp"
#include "UniScriptState.hpp"

#include "MersenneTwister.h"

extern "C"
{
    #include <lua.h>
    #include <lualib.h>
    #include <lauxlib.h>
}

using namespace cf::GameSys;


// Constants for m_StateOfExistence.
static const uint8_t StateOfExistence_Alive           = 0;
static const uint8_t StateOfExistence_Dead            = 1;
static const uint8_t StateOfExistence_FrozenSpectator = 2;
static const uint8_t StateOfExistence_FreeSpectator   = 3;


const char* ComponentHumanPlayerT::DocClass =
    "Entities with this component are associated with a client connection\n"
    "at whose end is a human player who provides input to control the entity.";


const cf::TypeSys::VarsDocT ComponentHumanPlayerT::DocVars[] =
{
    { "PlayerName",          "The name that the player chose for himself." },
    { "RandomCount",         "Keeps track of the next random number that is returned by the GetRandom() method." },
    { "State",               "For the player's main state machine, e.g. spectator, dead, alive, ..." },
    { "Health",              "Health." },
    { "Armor",               "Armor." },
    { "ActiveWeaponNr",      "The index number into the CarriedWeapon components of this entity, starting at 1, indicating the currently active weapon. The weapon must also be available (have been picked up) before the player can use it. A value of 0 means that \"no\" weapon is currently active." },
    { "NextWeaponNr",        "The next weapon to be drawn by SelectNextWeapon(). Like ActiveWeaponNr, this is an index number into the CarriedWeapon components of this entity, starting at 1. A value of 0 means \"none\"." },
    { "HaveItems",           "Bit field, entity can carry 32 different items." },
    { "HaveWeapons",         "Bit field, entity can carry 32 different weapons." },
    { "ActiveWeaponSlot",    "Index into m_HaveWeapons, m_HaveAmmoInWeapons, and for determining the weapon model index." },
    { "ActiveWeaponSequNr",  "The weapon anim sequence that we see (the local clients 1st person ('view') weapon model)." },
    { "ActiveWeaponFrameNr", "Respectively, this is the frame number of the current weapon sequence." },
    { "HaveAmmo",            "Entity can carry 16 different types of ammo (weapon independent). This is the amount of each." },
    { "HaveAmmoInWeapons",   "Entity can carry ammo in each of the 32 weapons. This is the amount of each." },
    { "HeadSway",            "The progress of one \"head swaying\" cycle in state FrozenSpectator." },
    { NULL, NULL }
};


ComponentHumanPlayerT::ComponentHumanPlayerT()
    : ComponentBaseT(),
      m_PlayerName("PlayerName", "Player"),
      m_RandomCount("RandomCount", 0),
      m_StateOfExistence("State", 2 /*StateOfExistence_FrozenSpectator*/),
      m_Health("Health", 100),
      m_Armor("Armor", 0),
      m_ActiveWeaponNr("ActiveWeaponNr", 0),
      m_NextWeaponNr("NextWeaponNr", 0),
      m_HaveItems("HaveItems", 0),
      m_HaveWeapons("HaveWeapons", 0),
      m_ActiveWeaponSlot("ActiveWeaponSlot", 0),
      m_ActiveWeaponSequNr("ActiveWeaponSequNr", 0),
      m_ActiveWeaponFrameNr("ActiveWeaponFrameNr", 0.0f),
      m_HaveAmmo("HaveAmmo", 16, 0),
      m_HaveAmmoInWeapons("HaveAmmoInWeapons", 32, 0),
      m_HeadSway("HeadSway", 0.0f),
      m_PlayerCommands(),
      m_GuiHUD(NULL)
{
    FillMemberVars();

    // TODO: Should rather store ParticleMaterialSetTs where their lifetime cannot be shorter than that of all particles...
    m_GenericMatSet = new ParticleMaterialSetT("generic", "Sprites/Generic1");
    m_WhiteSmokeMatSet = new ParticleMaterialSetT("white-smoke", "Sprites/smoke1/smoke_%02u");
}


ComponentHumanPlayerT::ComponentHumanPlayerT(const ComponentHumanPlayerT& Comp)
    : ComponentBaseT(Comp),
      m_PlayerName(Comp.m_PlayerName),
      m_RandomCount(Comp.m_RandomCount),
      m_StateOfExistence(Comp.m_StateOfExistence),
      m_Health(Comp.m_Health),
      m_Armor(Comp.m_Armor),
      m_ActiveWeaponNr(Comp.m_ActiveWeaponNr),
      m_NextWeaponNr(Comp.m_NextWeaponNr),
      m_HaveItems(Comp.m_HaveItems),
      m_HaveWeapons(Comp.m_HaveWeapons),
      m_ActiveWeaponSlot(Comp.m_ActiveWeaponSlot),
      m_ActiveWeaponSequNr(Comp.m_ActiveWeaponSequNr),
      m_ActiveWeaponFrameNr(Comp.m_ActiveWeaponFrameNr),
      m_HaveAmmo(Comp.m_HaveAmmo),
      m_HaveAmmoInWeapons(Comp.m_HaveAmmoInWeapons),
      m_HeadSway(Comp.m_HeadSway),
      m_PlayerCommands(),
      m_GuiHUD(NULL)
{
    FillMemberVars();

    // TODO: Should rather store ParticleMaterialSetTs where their lifetime cannot be shorter than that of all particles...
    m_GenericMatSet = new ParticleMaterialSetT("generic", "Sprites/Generic1");
    m_WhiteSmokeMatSet = new ParticleMaterialSetT("white-smoke", "Sprites/smoke1/smoke_%02u");
}


void ComponentHumanPlayerT::FillMemberVars()
{
    GetMemberVars().Add(&m_PlayerName);
    GetMemberVars().Add(&m_RandomCount);
    GetMemberVars().Add(&m_StateOfExistence);
    GetMemberVars().Add(&m_Health);
    GetMemberVars().Add(&m_Armor);
    GetMemberVars().Add(&m_ActiveWeaponNr);
    GetMemberVars().Add(&m_NextWeaponNr);
    GetMemberVars().Add(&m_HaveItems);
    GetMemberVars().Add(&m_HaveWeapons);
    GetMemberVars().Add(&m_ActiveWeaponSlot);
    GetMemberVars().Add(&m_ActiveWeaponSequNr);
    GetMemberVars().Add(&m_ActiveWeaponFrameNr);
    GetMemberVars().Add(&m_HaveAmmo);
    GetMemberVars().Add(&m_HaveAmmoInWeapons);
    GetMemberVars().Add(&m_HeadSway);
}


ComponentHumanPlayerT::~ComponentHumanPlayerT()
{
    delete m_WhiteSmokeMatSet;
    m_WhiteSmokeMatSet = NULL;

    delete m_GenericMatSet;
    m_GenericMatSet = NULL;
}


// The HUD GUI is intentionally not initialized in the constructors above, but only lazily
// when the GetGui() method is called, because in the constructors it is impossile to know
// or to learn if this ComponentHumanPlayerT instance is created on the server or the client,
// and if it is the local, first-person human player, or somebody else.
IntrusivePtrT<cf::GuiSys::GuiImplT> ComponentHumanPlayerT::GetGuiHUD()
{
    if (m_GuiHUD != NULL) return m_GuiHUD;

    // TODO:
    // if (on server) return NULL;
    // if (this human player is not first person instance) return NULL;

    static const char* FallbackGUI =
        "local gui = ...\n"
        "local Root = gui:new('WindowT', 'Root')\n"
        "gui:SetRootWindow(Root)\n"
        "\n"
        "function Root:OnInit()\n"
        "    self:GetTransform():set('Pos', 0, 0)\n"
        "    self:GetTransform():set('Size', 640, 480)\n"
        "\n"
        "    local c1 = gui:new('ComponentTextT')\n"
        "    c1:set('Text', [=====[%s]=====])\n"    // This is intended for use with e.g. wxString::Format().
        " -- c1:set('Font', 'Fonts/Impact')\n"
        "    c1:set('Scale', 0.3)\n"
        "    c1:set('Padding', 0, 0)\n"
        "    c1:set('Color', 15/255, 49/255, 106/255)\n"
        " -- c1:set('Alpha', 0.5)\n"
        "    c1:set('hor. Align', 0)\n"
        "    c1:set('ver. Align', 0)\n"
        "\n"
        "    local c2 = gui:new('ComponentImageT')\n"
        "    c2:set('Material', '')\n"
        "    c2:set('Color', 150/255, 170/255, 204/255)\n"
        "    c2:set('Alpha', 0.7)\n"
        "\n"
        "    self:AddComponent(c1, c2)\n"
        "\n"
        "    gui:activate      (true)\n"
        "    gui:setInteractive(false)\n"
        "    gui:showMouse     (false)\n"
        "    gui:setFocus      (Root)\n"
        "end\n";

    WorldT& World = GetEntity()->GetWorld();
    static const std::string GuiName = "Games/DeathMatch/GUIs/HUD_main.cgui";

    try
    {
        m_GuiHUD = new cf::GuiSys::GuiImplT(
            World.GetScriptState(),
            World.GetGuiResources());

        // Set the GUI object's "Player" field to the related component instance (`this`),
        // and the GUI object's "Entity" field to the related entity instance.
        // Expressed as pseudo code:
        //     gui.Player = this
        //     gui.Entity = this->GetEntity()
        {
            lua_State*    LuaState = World.GetScriptState().GetLuaState();
            StackCheckerT StackChecker(LuaState);
            ScriptBinderT Binder(LuaState);

            Binder.Push(m_GuiHUD);
            Binder.Push(IntrusivePtrT<ComponentHumanPlayerT>(this));
            lua_setfield(LuaState, -2, "Player");
            Binder.Push(IntrusivePtrT<EntityT>(GetEntity()));
            lua_setfield(LuaState, -2, "Entity");
            lua_pop(LuaState, 1);
        }

        m_GuiHUD->LoadScript(GuiName);

        // Active status is not really relevant for our Gui that is not managed by the GuiMan,
        // but still make sure that clock tick events are properly propagated to all windows.
        m_GuiHUD->Activate();
    }
    catch (const cf::GuiSys::GuiImplT::InitErrorT& IE)
    {
        // Need a new GuiImplT instance here, as the one allocated above is in unknown state.
        m_GuiHUD = new cf::GuiSys::GuiImplT(
            World.GetScriptState(),
            World.GetGuiResources());

        // This one must not throw again...
        m_GuiHUD->LoadScript(
            cf::String::Replace(FallbackGUI, "%s", "Could not load GUI\n" + GuiName + "\n\n" + IE.what()),
            cf::GuiSys::GuiImplT::InitFlag_InlineCode);
    }

    return m_GuiHUD;
}


Vector3dT ComponentHumanPlayerT::GetPlayerVelocity() const
{
    if (!GetEntity())
        return Vector3dT();

    IntrusivePtrT<ComponentPlayerPhysicsT> CompPlayerPhysics =
        dynamic_pointer_cast<ComponentPlayerPhysicsT>(GetEntity()->GetComponent("PlayerPhysics"));

    if (CompPlayerPhysics == NULL)
        return Vector3dT();

    return CompPlayerPhysics->GetVelocity();
}


Vector3dT ComponentHumanPlayerT::GetCameraOriginWS() const
{
    if (!GetEntity())
        return Vector3dT();

    if (GetEntity()->GetChildren().Size() == 0)
        return GetEntity()->GetTransform()->GetOriginWS().AsVectorOfDouble();

    // The normal, expected case: Use the entity's camera transform.
    return GetEntity()->GetChildren()[0]->GetTransform()->GetOriginWS().AsVectorOfDouble();
}


Vector3dT ComponentHumanPlayerT::GetCameraViewDirWS(double Random) const
{
    if (!GetEntity()) return Vector3dT();

    IntrusivePtrT<ComponentTransformT> Trafo = GetEntity()->GetTransform();

    if (GetEntity()->GetChildren().Size() > 0)
    {
        // The normal, expected case: Use the entity's camera transform.
        Trafo = GetEntity()->GetChildren()[0]->GetTransform();
    }

    const cf::math::Matrix3x3fT Mat(Trafo->GetQuatWS());

    Vector3dT ViewDir = Mat.GetAxis(0).AsVectorOfDouble();

    if (Random > 0.0)
    {
        ViewDir += Mat.GetAxis(1).AsVectorOfDouble() * ((rand() % 10000 - 5000) / 5000.0) * Random;
        ViewDir += Mat.GetAxis(2).AsVectorOfDouble() * ((rand() % 10000 - 5000) / 5000.0) * Random;

        ViewDir = normalizeOr0(ViewDir);
    }

    return ViewDir;
}


RayResultT ComponentHumanPlayerT::TraceCameraRay(const Vector3dT& Dir) const
{
    if (!GetEntity())
        return RayResultT(NULL);

    IntrusivePtrT<ComponentPhysicsT> Physics =
        dynamic_pointer_cast<ComponentPhysicsT>(GetEntity()->GetComponent("Physics"));

    RayResultT RayResult(Physics != NULL ? Physics->GetRigidBody() : NULL);

    GetEntity()->GetWorld().GetPhysicsWorld()->TraceRay(
        UnitsToPhys(GetCameraOriginWS()),
        Dir * 9999.0, RayResult);

    return RayResult;
}


void ComponentHumanPlayerT::InflictDamage(EntityT* OtherEnt, float Amount, const Vector3dT& Dir) const
{
    if (!OtherEnt) return;

    IntrusivePtrT<ComponentScriptT> OtherScript =
        dynamic_pointer_cast<ComponentScriptT>(OtherEnt->GetComponent("Script"));

    if (OtherScript == NULL) return;

    cf::ScriptBinderT      OtherBinder(OtherEnt->GetWorld().GetScriptState().GetLuaState());
    IntrusivePtrT<EntityT> This = GetEntity();

    OtherBinder.Push(This);

    OtherScript->CallLuaMethod("TakeDamage", 1, "ffff", Amount, Dir.x, Dir.y, Dir.z);
}


void ComponentHumanPlayerT::PostEvent(unsigned int EventType) const
{
    IntrusivePtrT<ComponentScriptT> Script =
        dynamic_pointer_cast<ComponentScriptT>(GetEntity()->GetComponent("Script"));

    // This assumes that in the Script component's script ("HumanPlayer.lua"),
    // script method InitEventTypes() has been called.
    if (Script != NULL)
        Script->PostEvent(EventType);
}


ComponentHumanPlayerT* ComponentHumanPlayerT::Clone() const
{
    return new ComponentHumanPlayerT(*this);
}


void ComponentHumanPlayerT::CheckGUIs(bool ThinkingOnServerSide, bool HaveButtonClick) const
{
    ArrayT< IntrusivePtrT<EntityT> > AllEnts;

    GetEntity()->GetWorld().GetRootEntity()->GetAll(AllEnts);

    for (unsigned int EntNr = 0; EntNr < AllEnts.Size(); EntNr++)
    {
        if (AllEnts[EntNr] == GetEntity()) continue;    // We don't touch us ourselves.

        // Test if maybe we're near a static detail model with an interactive GUI.
        const ArrayT< IntrusivePtrT<ComponentBaseT> >& Components = AllEnts[EntNr]->GetComponents();

        // TODO: We iterate over each component of each entity here... can this somehow be reduced?
        //       For example hy having the world keep a list that is updated only once during init?
        for (unsigned int CompNr = 0; CompNr < Components.Size(); CompNr++)
        {
            if (Components[CompNr]->GetType() != &ComponentModelT::TypeInfo) continue;

            IntrusivePtrT<ComponentModelT> CompModel = static_pointer_cast<ComponentModelT>(Components[CompNr]);

            // TODO: Also deal with the GUI when this is a REPREDICTION run???
            //
            // Answer: Normally not, because what is done during prediction is only for eye candy,
            // and repeating it in REprediction would e.g. trigger/schedule interpolations *twice* (or in fact even more often).
            //
            // On the other hand, compare this to what happens when the player e.g. enters his name into a text field.
            // The string with the name would be part of the "relevant GUI state" (state that is sync'ed over the network).
            // As such, the string would ONLY be handled correctly when REprediction runs are applies to GUIs as they are applied
            // to HumanPlayerTs (assuming the string is also handled in normal initial prediction).
            // Example: The player enters "abc" on the client and prediction updates the string, but the server then sends a message
            // that the player was force-moved by an explosion and the "abc" string was actually typed into the wall next to the GUI.
            //
            // ==> Either we have to run prediction AND REpredection with the GUIs,
            //     OR we treat them like any other entity and ONLY update them on the server-side.
            //
            // ==> Conflict of interests: Only if the GUIs interpolation timers were a part of the "GUI state" would they work properly,
            //     (which doesn't make much sense), or if we ran GUIs in prediction (but not REprediction) only (no good in the above example).
            //
            // ==> Solution: Update the relevant GUI state only ever on the server-side, and run GUI updates in prediction only
            //               (but never in REprediction).
            //
            // ==> How do we separate the two???
            //     ...

            // 1. Has this component an interactive GUI at all?
            IntrusivePtrT<cf::GuiSys::GuiImplT> Gui = CompModel->GetGui();

            if (Gui.IsNull()) continue;
            if (!Gui->GetIsInteractive()) continue;


            // 2. Can we retrieve the plane of its screen panel?
            Vector3fT GuiOrigin;
            Vector3fT GuiAxisX;
            Vector3fT GuiAxisY;

            AnimPoseT* Pose = CompModel->GetPose();

            if (!Pose) continue;
            if (!Pose->GetGuiPlane(0, GuiOrigin, GuiAxisX, GuiAxisY)) continue;

            const MatrixT M2W = CompModel->GetEntity()->GetTransform()->GetEntityToWorld();

            GuiOrigin = M2W.Mul1(GuiOrigin);
            GuiAxisX  = M2W.Mul0(GuiAxisX);
            GuiAxisY  = M2W.Mul0(GuiAxisY);


            // 3. Are we looking roughly into the screen normal?
            const Vector3fT GuiNormal = normalize(cross(GuiAxisY, GuiAxisX), 0.0f);
            const Plane3fT  GuiPlane  = Plane3fT(GuiNormal, dot(GuiOrigin, GuiNormal));
            const Vector3fT ViewDir   = GetCameraViewDirWS().AsVectorOfFloat();

            if (-dot(ViewDir, GuiPlane.Normal) < 0.001f) continue;


            // 4. Does our view ray hit the screen panel?
            // (I've obtained the equation for r by rolling the corresponding Plane3T<T>::GetIntersection() method out.)
            const Vector3fT OurOrigin = GetCameraOriginWS().AsVectorOfFloat();      // TODO: don't convert from float to double to float.
            const float     r         = (GuiPlane.Dist - dot(GuiPlane.Normal, OurOrigin)) / dot(GuiPlane.Normal, ViewDir);

            if (r < 0.0f || r > 160.0f) continue;

            const Vector3fT HitPos = OurOrigin + ViewDir * r;

            // Project HitPos into the GUI plane, in order to obtain the 2D coordinate in the GuiSys' virtual pixel space (640x480).
            float px = dot(HitPos - GuiOrigin, GuiAxisX) / GuiAxisX.GetLengthSqr();
            float py = dot(HitPos - GuiOrigin, GuiAxisY) / GuiAxisY.GetLengthSqr();

            if (px < -0.5f || px > 1.5f) continue;
            if (py < -0.5f || py > 1.5f) continue;

            if (px < 0.0f) px = 0.0f; if (px > 0.99f) px = 0.99f;
            if (py < 0.0f) py = 0.0f; if (py > 0.98f) py = 0.98f;


            // TODO: Trace against walls!
            // TODO: Is somebody else using this GUI, too? It is useful to check this in order to avoid "race conditions"
            //       on the server, with two entities competing for the mouse pointer, causing frequent pointer "jumps"
            //       and thus possibly building up an ever growing set of interpolations in each frame.
            Gui->SetMousePos(px * 640.0f, py * 480.0f);

            CaMouseEventT ME;

            ME.Type  =CaMouseEventT::CM_MOVE_X;
            ME.Amount=0;

            Gui->ProcessDeviceEvent(ME);

            // Process mouse button events only on the server side.
            // Note that this is a somewhat *arbitrary* compromise to the question "Where do we stop / how far do we go in"
            // client prediction.
            // Drawing the line here means that GUIs should be programmed in a way such that mouse movements do *not* affect
            // world state -- only mouse clicks can do that. (In fact, we should probably also keep mouse movement events from
            // the GUI when this is a *reprediction* run, as detailed in the (much older) comment above.)
            if (ThinkingOnServerSide)
            {
                if (HaveButtonClick)
                {
                    ME.Type = CaMouseEventT::CM_BUTTON0;

                    ME.Amount = 1;  // Button down.
                    Gui->ProcessDeviceEvent(ME);

                    ME.Amount = 0;  // Button up.
                    Gui->ProcessDeviceEvent(ME);
                }
            }
        }
    }
}


void ComponentHumanPlayerT::Think(const PlayerCommandT& PlayerCommand, bool ThinkingOnServerSide)
{
    IntrusivePtrT<ComponentPlayerPhysicsT> CompPlayerPhysics = dynamic_pointer_cast<ComponentPlayerPhysicsT>(GetEntity()->GetComponent("PlayerPhysics"));
    IntrusivePtrT<ComponentModelT> Model3rdPerson = dynamic_pointer_cast<ComponentModelT>(GetEntity()->GetComponent("Model"));

    if (CompPlayerPhysics == NULL) return;      // The presence of CompPlayerPhysics is mandatory...
    if (Model3rdPerson == NULL) return;         // The presence of CompPlayerPhysics is mandatory...

    switch (GetStateOfExistence())
    {
        case StateOfExistence_Alive:
        {
            // Update Heading
            {
                cf::math::AnglesfT Angles(GetEntity()->GetTransform()->GetQuatPS());       // We actually rotate the entity.

                Angles.yaw() -= PlayerCommand.DeltaHeading / 8192.0f * 45.0f;

                if (PlayerCommand.Keys & PCK_TurnLeft ) Angles.yaw() += 120.0f * PlayerCommand.FrameTime;
                if (PlayerCommand.Keys & PCK_TurnRight) Angles.yaw() -= 120.0f * PlayerCommand.FrameTime;

                Angles.pitch() = 0.0f;
                Angles.roll()  = 0.0f;

                GetEntity()->GetTransform()->SetQuatPS(cf::math::QuaternionfT(Angles));
            }

            // Update Pitch and Bank
            {
                cf::math::AnglesfT Angles(GetEntity()->GetChildren()[0]->GetTransform()->GetQuatPS());     // We update the camera, not the entity.

                const int dp = PlayerCommand.DeltaPitch;
                Angles.pitch() += (dp < 32768 ? dp : dp - 65536) / 8192.0f * 45.0f;

                if (PlayerCommand.Keys & PCK_LookUp  ) Angles.pitch() -= 120.0f * PlayerCommand.FrameTime;
                if (PlayerCommand.Keys & PCK_LookDown) Angles.pitch() += 120.0f * PlayerCommand.FrameTime;

                if (Angles.pitch() >  90.0f) Angles.pitch() =  90.0f;
                if (Angles.pitch() < -90.0f) Angles.pitch() = -90.0f;

                const int db = PlayerCommand.DeltaBank;
                Angles.roll() += (db < 32768 ? db : db - 65536) / 8192.0f * 45.0f;

                if (PlayerCommand.Keys & PCK_CenterView)
                {
                    Angles.yaw()   = 0.0f;
                    Angles.pitch() = 0.0f;
                    Angles.roll()  = 0.0f;
                }

                GetEntity()->GetChildren()[0]->GetTransform()->SetQuatPS(cf::math::QuaternionfT(Angles));
            }


            VectorT             WishVelocity;
            bool                WishJump=false;
            const Vector3dT     Vel     = cf::math::Matrix3x3fT(GetEntity()->GetTransform()->GetQuatWS()).GetAxis(0).AsVectorOfDouble() * 240.0;
            const unsigned long Keys    =PlayerCommand.Keys;

            if (Keys & PCK_MoveForward ) WishVelocity=             VectorT( Vel.x,  Vel.y, 0);
            if (Keys & PCK_MoveBackward) WishVelocity=WishVelocity+VectorT(-Vel.x, -Vel.y, 0);
            if (Keys & PCK_StrafeLeft  ) WishVelocity=WishVelocity+VectorT(-Vel.y,  Vel.x, 0);
            if (Keys & PCK_StrafeRight ) WishVelocity=WishVelocity+VectorT( Vel.y, -Vel.x, 0);

            if (Keys & PCK_Jump        ) WishJump=true;
         // if (Keys & PCK_Duck        ) ;
            if (Keys & PCK_Walk        ) WishVelocity=scale(WishVelocity, 0.5);

            VectorT       WishVelLadder;
            const VectorT ViewLadder = GetCameraViewDirWS() * 150.0;

            // TODO: Also take LATERAL movement into account.
            // TODO: All this needs a HUGE clean-up! Can probably put a lot of this stuff into Physics::MoveHuman.
            if (Keys & PCK_MoveForward ) WishVelLadder=WishVelLadder+ViewLadder;
            if (Keys & PCK_MoveBackward) WishVelLadder=WishVelLadder-ViewLadder;
            if (Keys & PCK_Walk        ) WishVelLadder=scale(WishVelLadder, 0.5);

            /*if (Clients[ClientNr].move_noclip)
            {
                // This code was simply changed and rewritten until it "worked".
                // May still be buggy anyway.
                double RadPitch=double(m_Pitch)/32768.0*3.141592654;
                double Fak     =VectorDot(WishVelocity, VectorT(LookupTables::Angle16ToSin[m_Heading], LookupTables::Angle16ToCos[m_Heading], 0));

                WishVelocity.x*=cos(RadPitch);
                WishVelocity.y*=cos(RadPitch);
                WishVelocity.z=-sin(RadPitch)*Fak;

                m_Origin=m_Origin+scale(WishVelocity, PlayerCommand.FrameTime);

                // TODO: If not already done on state change (--> "noclip"), set the model sequence to "swim".  ;-)
            }
            else */
            {
                VectorT XYVel    = CompPlayerPhysics->GetVelocity(); XYVel.z = 0;
                double  OldSpeed = length(XYVel);

                CompPlayerPhysics->SetMember("StepHeight", 18.5);
                CompPlayerPhysics->MoveHuman(PlayerCommand.FrameTime, WishVelocity.AsVectorOfFloat(), WishVelLadder.AsVectorOfFloat(), WishJump);

                XYVel = CompPlayerPhysics->GetVelocity(); XYVel.z = 0;
                double NewSpeed = length(XYVel);

                if (OldSpeed <= 40.0 && NewSpeed > 40.0) Model3rdPerson->SetMember("Animation", 3);
                if (OldSpeed >= 40.0 && NewSpeed < 40.0) Model3rdPerson->SetMember("Animation", 1);
            }

            // GameWorld->ModelAdvanceFrameTime() is called on client side in Draw().


            // Check if any key for firing the currently active weapon was pressed.
            IntrusivePtrT<ComponentCarriedWeaponT> CarriedWeapon = GetActiveWeapon();

            if (CarriedWeapon != NULL)
            {
                if (PlayerCommand.Keys & PCK_Fire1) CarriedWeapon->CallLuaMethod("FirePrimary",   0, "b", ThinkingOnServerSide);
                if (PlayerCommand.Keys & PCK_Fire2) CarriedWeapon->CallLuaMethod("FireSecondary", 0, "b", ThinkingOnServerSide);
            }

            // Check if any key for changing the current weapon was pressed.
            if (Keys >> 28)
            {
                IntrusivePtrT<ComponentScriptT> Script =
                    dynamic_pointer_cast<ComponentScriptT>(GetEntity()->GetComponent("Script"));

                if (Script != NULL)
                    Script->CallLuaMethod("ChangeWeapon", 0, "i", Keys >> 28);
            }

            // Check if any GUIs must be updated.
            CheckGUIs(ThinkingOnServerSide, (Keys >> 28) == 10);
            break;
        }

        case StateOfExistence_Dead:
        {
            const float MIN_CAMERA_HEIGHT = -20.0f;

            CompPlayerPhysics->SetMember("StepHeight", 4.0);
            CompPlayerPhysics->MoveHuman(PlayerCommand.FrameTime, Vector3fT(), Vector3fT(), false);

            IntrusivePtrT<ComponentTransformT> CameraTrafo = GetEntity()->GetChildren()[0]->GetTransform();

            if (CameraTrafo->GetOriginPS().z > MIN_CAMERA_HEIGHT)
            {
                // We only update the camera, not the entity.
                Vector3fT          Origin(CameraTrafo->GetOriginPS());
                cf::math::AnglesfT Angles(CameraTrafo->GetQuatPS());

                Origin.z -= 80.0f * PlayerCommand.FrameTime;
                Angles.roll() += PlayerCommand.FrameTime * 200.0f;

                CameraTrafo->SetOriginPS(Origin);
                CameraTrafo->SetQuatPS(cf::math::QuaternionfT(Angles));
            }

            // We entered this state after we died. Leave it after we have come
            // to a complete halt, and the death sequence is over.
            // TODO: In the velocity check, `< 0.1` is sometimes never reached -- examine why.
            if (CameraTrafo->GetOriginPS().z <= MIN_CAMERA_HEIGHT && length(CompPlayerPhysics->GetVelocity()) < 0.2 /* && TODO: Is death anim sequence over?? */)
            {
                // On the server, create a new "corpse" entity in the place where we died,
                // or else it seems to other players like the model disappears when we respawn.
                if (ThinkingOnServerSide)
                {
                    IntrusivePtrT<ComponentModelT> PlayerModelComp = dynamic_pointer_cast<ComponentModelT>(GetEntity()->GetComponent("Model"));

                    if (PlayerModelComp != NULL)
                    {
                        IntrusivePtrT<EntityT> Ent = new EntityT(EntityCreateParamsT(GetEntity()->GetWorld()));
                        GetEntity()->GetParent()->AddChild(Ent);

                        Ent->GetTransform()->SetOriginPS(GetEntity()->GetTransform()->GetOriginPS());
                        Ent->GetTransform()->SetQuatPS(GetEntity()->GetTransform()->GetQuatPS());

                        IntrusivePtrT<ComponentModelT> ModelComp = new ComponentModelT(*PlayerModelComp);
                        Ent->AddComponent(ModelComp);

                        // TODO: Disappear when some condition is met (timeout, not in anyones PVS, alpha fade-out, too many corpses, ...)
                        // TODO: Decompose to gibs when hit by a rocket.
                    }
                }

                // m_HeadSway must restart at 0.0 when the FrozenSpectator state is entered.
                m_HeadSway.Set(0.0f);
                SetStateOfExistence(StateOfExistence_FrozenSpectator);
            }

            break;
        }

        case StateOfExistence_FrozenSpectator:
        {
            // Make it look as if our head is swaying in mild wind while we're waiting in this state.
            // The resulting movement makes the view a lot more pleasing than standing utterly still.
            {
                const float Pi           = 3.14159265359f;
                const float SecPerSwing  = 15.0f;
                const float PC_FrameTime = std::min(PlayerCommand.FrameTime, 0.05f);    // Avoid jumpiness with very low FPS.

                float SwayTime   = m_HeadSway.Get();
                float SwingAngle = sin(SwayTime) * 1.1f;    // +/- 1.1°

                cf::math::AnglesfT Angles(GetEntity()->GetChildren()[0]->GetTransform()->GetQuatPS());  // We update the camera, not the entity.

                // Remove our previous addition to the camera orientation.
                Angles.yaw()  -= SwingAngle;
                Angles.roll() -= SwingAngle;

                SwayTime += 2.0f*Pi*PC_FrameTime / SecPerSwing;

                if (SwayTime > 6.3f) SwayTime -= 2.0f*Pi;

                // Add our new addition to the camera orientation.
                SwingAngle = sin(SwayTime) * 1.1f;    // +/- 1.1°

                Angles.yaw()  += SwingAngle;
                Angles.roll() += SwingAngle;

                m_HeadSway.Set(SwayTime);
                GetEntity()->GetChildren()[0]->GetTransform()->SetQuatPS(cf::math::QuaternionfT(Angles));
            }

            // TODO: We want the player to release the button between respawns in order to avoid permanent "respawn-flickering"
            //       that otherwise may occur if the player keeps the button continuously pressed down.
            //       These are the same technics that also apply to the "jump"-button.
            if ((PlayerCommand.Keys & PCK_Fire1)==0) break;  // "Fire" button not pressed.

            ArrayT< IntrusivePtrT<EntityT> > AllEnts;
            GetEntity()->GetWorld().GetRootEntity()->GetAll(AllEnts);

            // The "Fire"-button was pressed. Now try to determine a free "InfoPlayerStart" entity for respawning there.
            for (unsigned int EntNr = 0; EntNr < AllEnts.Size(); EntNr++)
            {
                IntrusivePtrT<EntityT> IPSEntity = AllEnts[EntNr];

                if (IPSEntity->GetComponent("PlayerStart") == NULL) continue;

                const BoundingBox3dT Dimensions(Vector3dT(-16.0, -16.0, -36.0), Vector3dT(16.0,  16.0, 36.0));

                // This is actually an "InfoPlayerStart" entity. Now try to put our own bounding box at the origin of 'IPSEntity',
                // but try to correct/choose the height such that we are on ground (instead of hovering above it).
                Vector3dT OurNewOrigin = IPSEntity->GetTransform()->GetOriginWS().AsVectorOfDouble();

                // First, create a BB of dimensions (-16.0, -16.0, -4.0) - (16.0, 16.0, 4.0).
                const BoundingBox3T<double> ClearingBB(VectorT(Dimensions.Min.x, Dimensions.Min.y, -Dimensions.Max.z), Dimensions.Max);

                cf::ClipSys::ClipModelT* IgnorePlayerClipModel = NULL;  // TODO!

                // Move ClearingBB up to a reasonable height (if possible!), such that the *full* BB (that is, Dimensions) is clear of (not stuck in) solid.
                cf::ClipSys::TraceResultT Result(1.0);
                GetEntity()->GetWorld().GetClipWorld()->TraceBoundingBox(ClearingBB, OurNewOrigin, VectorT(0.0, 0.0, 120.0), MaterialT::Clip_Players, IgnorePlayerClipModel, Result);
                const double AddHeight=120.0*Result.Fraction;

                // Move ClearingBB down as far as possible.
                Result=cf::ClipSys::TraceResultT(1.0);
                GetEntity()->GetWorld().GetClipWorld()->TraceBoundingBox(ClearingBB, OurNewOrigin+VectorT(0.0, 0.0, AddHeight), VectorT(0.0, 0.0, -1000.0), MaterialT::Clip_Players, IgnorePlayerClipModel, Result);
                const double SubHeight=1000.0*Result.Fraction;

                // Beachte: Hier für Epsilon 1.0 (statt z.B. 1.23456789) zu wählen hebt u.U. GENAU den (0 0 -1) Test in
                // Physics::CategorizePosition() auf! Nicht schlimm, wenn aber auf Client-Seite übers Netz kleine Rundungsfehler
                // vorhanden sind (es werden floats übertragen, nicht doubles!), kommt CategorizePosition() u.U. auf Client- und
                // Server-Seite zu verschiedenen Ergebnissen! Der Effekt spielt sich zwar in einem Intervall der Größe 1.0 ab,
                // kann mit OpenGL aber zu deutlichem Pixel-Flimmern führen!
                OurNewOrigin.z = OurNewOrigin.z + AddHeight - SubHeight + (ClearingBB.Min.z - Dimensions.Min.z/*1628.8*/) + 0.123456789/*Epsilon (sonst Ruckeln am Anfang!)*/;

                // Old, deprecated code (can get us stuck in non-level ground).
                // const double HeightAboveGround=GameWorld->MapClipLine(OurNewOrigin, VectorT(0, 0, -1.0), 0, 999999.9);
                // OurNewOrigin.z = OurNewOrigin.z - HeightAboveGround - Dimensions.Min.z + 1.23456789/*Epsilon (needed to avoid ruggy initial movement!)*/;


                BoundingBox3dT OurBB(Dimensions);

                OurBB.Min+=OurNewOrigin;
                OurBB.Max+=OurNewOrigin;

                ArrayT<cf::ClipSys::ClipModelT*> ClipModels;
                GetEntity()->GetWorld().GetClipWorld()->GetClipModelsFromBB(ClipModels, MaterialT::Clip_Players, OurBB);

                if (ClipModels.Size() == 0)
                {
                    // A suitable "InfoPlayerStart" entity was found -- respawn!
                    GetEntity()->GetTransform()->SetOriginWS(OurNewOrigin.AsVectorOfFloat());
                    GetEntity()->GetTransform()->SetQuatWS(IPSEntity->GetTransform()->GetQuatWS());  // TODO: Can we make sure that the z-axis points straight up, i.e. bank and pitch are 0?
                    GetEntity()->GetChildren()[0]->GetTransform()->SetOriginPS(Vector3fT(0.0f, 0.0f, 32.0f));    // TODO: Hardcoded values here and in the server code that creates the entity...
                    GetEntity()->GetChildren()[0]->GetTransform()->SetQuatPS(cf::math::QuaternionfT());
                    SetStateOfExistence(StateOfExistence_Alive);
                    Model3rdPerson->SetMember("Animation", 0);
                    SetHealth(100);
                    SetArmor(0);
                    m_ActiveWeaponNr.Set(0);
                    m_NextWeaponNr.Set(0);
                    SetHaveItems(0);
                    SetHaveWeapons(0);          // TODO: Iterate over the carried weapons, and reset their `IsAvail` flag to `false`?
                    SetActiveWeaponSlot(0);
                    SetActiveWeaponSequNr(0);
                    SetActiveWeaponFrameNr(0.0f);
                    m_HeadSway.Set(0.0f);

                    IntrusivePtrT<ComponentCollisionModelT> CompCollMdl = dynamic_pointer_cast<ComponentCollisionModelT>(GetEntity()->GetComponent("CollisionModel"));

                    if (CompCollMdl != NULL)
                        CompCollMdl->SetBoundingBox(Dimensions, "Textures/meta/collisionmodel");

                    for (char Nr=0; Nr<15; Nr++) GetHaveAmmo()[Nr]=0;   // IMPORTANT: Do not clear the frags value in 'HaveAmmo[AMMO_SLOT_FRAGS]'!
                    for (char Nr=0; Nr<32; Nr++) GetHaveAmmoInWeapons()[Nr]=0;

                    break;
                }
            }

            break;
        }

        case StateOfExistence_FreeSpectator:
            break;
    }


    // Mirror the camera's orientation to the child entity that holds our 1st-person weapon model.
    IntrusivePtrT<const ComponentTransformT> CameraTrafo = GetEntity()->GetChildren()[0]->GetTransform();

    GetEntity()->GetChildren()[1]->GetTransform()->SetQuatPS(CameraTrafo->GetQuatPS());
}


IntrusivePtrT<ComponentCarriedWeaponT> ComponentHumanPlayerT::GetActiveWeapon() const
{
    if (m_ActiveWeaponNr.Get() == 0) return NULL;

    IntrusivePtrT<ComponentCarriedWeaponT> CarriedWeapon =
        dynamic_pointer_cast<ComponentCarriedWeaponT>(GetEntity()->GetComponent("CarriedWeapon", m_ActiveWeaponNr.Get() - 1));

    if (CarriedWeapon == NULL) return NULL;
    if (!CarriedWeapon->IsAvail()) return NULL;

    return CarriedWeapon;
}


void ComponentHumanPlayerT::SelectWeapon(uint8_t NextWeaponNr, bool Force)
{
    // If the requested weapon is already active, there is nothing to do.
    if (m_ActiveWeaponNr.Get() == NextWeaponNr) return;

    // Holster the currently active weapon.
    // The current weapon will, at the end of its holstering sequence, make sure that SelectNextWeapon() is called.
    IntrusivePtrT<ComponentCarriedWeaponT> CarriedWeapon = GetActiveWeapon();

    if (CarriedWeapon == NULL || Force)
    {
        // We get here whenever there was a problem with finding the currently carried weapon
        // (where m_ActiveWeaponNr.Get() == NONE is a special case thereof), or if the carried
        // weapon was technically found, but is not available to (was never picked up by) the player.
        m_NextWeaponNr.Set(NextWeaponNr);
        SelectNextWeapon();
        return;
    }

    bool IsIdle = true;
    CarriedWeapon->CallLuaMethod("IsIdle", 0, ">b", &IsIdle);

    if (!IsIdle)
    {
        // If the currently active weapon is not idle, we cannot select another weapon,
        // and thus have to ignore the related request.
        return;
    }

    bool IsHolstering = false;
    CarriedWeapon->CallLuaMethod("Holster", 0, ">b", &IsHolstering);

    if (!IsHolstering)
    {
        // The current weapon may not support holstering (e.g. not having a holstering animation).
        // In this case, select the next weapon immediately.
        m_NextWeaponNr.Set(NextWeaponNr);
        SelectNextWeapon();
        return;
    }

    // The call to Holster() above was successful, the weapon is now holstering itself.
    // When the end of the holstering sequence is reached, the weapons's OnSequenceWrap[_Sv]()
    // method will be called, which in turn will call SelectNextWeapon() to draw the next weapon.
    m_NextWeaponNr.Set(NextWeaponNr);
}


void ComponentHumanPlayerT::SelectNextWeapon()
{
    m_ActiveWeaponNr.Set(m_NextWeaponNr.Get());

    IntrusivePtrT<ComponentCarriedWeaponT> CarriedWeapon = GetActiveWeapon();

    if (CarriedWeapon == NULL)
    {
        IntrusivePtrT<ComponentModelT> Model1stPerson = dynamic_pointer_cast<ComponentModelT>(GetEntity()->GetChildren()[1]->GetComponent("Model"));

        if (Model1stPerson != NULL)
        {
            // Update the 1st-person weapon model for "no weapon".
            Model1stPerson->SetMember("Show", false);
            Model1stPerson->SetMember("Name", std::string(""));
        }

        return;
    }

    CarriedWeapon->CallLuaMethod("Draw", 0);
}


namespace
{
    ArrayT<unsigned int> s_RandomPool;

    void InitRandomPool()
    {
        if (s_RandomPool.Size() == 0)
        {
            MTRand RNG;

            s_RandomPool.PushBackEmpty(1024);

            for (unsigned int i = 0; i < s_RandomPool.Size(); i++)
                s_RandomPool[i] = RNG.randInt();
        }
    }
}


unsigned int ComponentHumanPlayerT::GetRandom(unsigned int n)
{
    if (n == 0) return 0;

    InitRandomPool();

    const unsigned int r = s_RandomPool[m_RandomCount.Get() % s_RandomPool.Size()] % n;

    m_RandomCount.Set(m_RandomCount.Get() + 1);

    return r;
}


double ComponentHumanPlayerT::GetRandom()
{
    InitRandomPool();

    const double r = double(s_RandomPool[m_RandomCount.Get() % s_RandomPool.Size()]) * (1.0 / 4294967295.0);

    m_RandomCount.Set(m_RandomCount.Get() + 1);

    return r;
}


BoundingBox3fT ComponentHumanPlayerT::GetVisualBB() const
{
    const float r = 1.0f;

    // Make sure that the EntityT::GetVisualBB() method always returns *some* bounding-box if the entity is a
    // human player, even if for whatever reason there is no other component (e.g. a Model) that adds one.
    // This makes (doubly) sure that in the `CaServerWorldT`, a client is always found in its own PVS.
    return BoundingBox3fT(Vector3fT(-r, -r, -r), Vector3fT(r, r, r));
}


void ComponentHumanPlayerT::DoServerFrame(float t)
{
    // **********************************************************************************************************************************************
    // IMPORTANT NOTE:
    //
    // Although the Think() functions of all entity classes (children of BaseEntityT) are only ever called on the SERVER-SIDE,
    // and you should always think of them as ONLY EVER BEING CALLED ON THE SERVER-SIDE, the truth is that there is an exception:
    // For EntHumanPlayerT entities (but no other entity classes!), this function is also called on the client-side for prediction.
    // You should still think of this function as ONLY EVER BEING CALLED ON THE SERVER-SIDE,
    // but the above mentioned exception implies that this function must always be deterministic (that is, reproducible).
    // That especially means:
    // 1. Calls to rand()-like functions are NOT allowed! Rather, you have to use something like
    //        LookupTables::RandomUShort[PlayerCommands[PCNr].Nr & 0xFFF]
    //    in order to obtain pseudo-random numbers from PlayerCommands[PCNr].Nr. Related examples are found in the code below.
    // 2. This function must NOT have a state besides the member variable 'EntHumanPlayerT::State'.
    //    That is, you can NOT introduce and use additional private or protected member variables, because the
    //    client-side prediction will only take the State variable (and no additional private variables) into account!
    //    If you did anyway, this function would not be reproducible by the State variable alone,
    //    and thus violate the most vital requirement for prediction.
    //    As a solution, do only use member variables of State for all your state management!
    //    This seems sometimes problematic and like working-around-the-corner, but you will find that very often you can exploit
    //    one or more of the existing variables in State for your purposes! Examples are found in the code below,
    //    where the next weapon animation sequence is simply determined by the previous weapon animation sequence!
    //    If that in rare cases does not work, use for example one of the unused HaveAmmo or HaveAmmoInWeapon variables.
    //    It should also be noted that having *constant* state is perfectly acceptable.
    //    For example, if you load something from disk and/or precalculate something in the constructor (thus both on client and server side),
    //    it is perfectly fine if you refer to this data later from Think() - just don't modify it!
    //    I make heavy use of this feature in the MOD for the USAF, where complicated spatial paths are precalculated and stored as "constant state".
    //
    // Another special case (not related to prediction) is the fact that the Think()ing of EntHumanPlayerT entities
    // is driven by PlayerCommands, and NOT by the usual FrameTime and ServerFrameNr parameters of this function.
    // It is the task of this function to process all the PlayerCommands that are available
    // (fed by the server by calling the EntHumanPlayerT::ProcessConfigString() function appropriately),
    // and thus the regular parameters 'FrameTime' and 'ServerFrameNr' MUST NOT BE USED!
    // (Moreover, the client-side prediction has no idea what to pass-in here anyway, and thus always passes 'FrameTime=-1.0' and 'ServerFrameNr=0'.)
    //
    // Unfortunately, there is an exception to the exception:
    // Sometimes, we want to or have to do things that should only occur on server-side (and thus be exempted from prediction), like
    // a) modifying other (usually non-predicted) entities (like calling another entities TakeDamage() function), or
    // b) creating new entities (e.g. a thrown face-hugger, a fired missile, ...) (not possible on client-side).
    // Against my initial statement above, this requires a concious consideration if this is a regular server-side thinking call,
    // or a client-side prediction call.
    // Whether we are on server-side or not is determined by the ThinkingOnServerSide variable as shown below.
    // The code below also has examples (partially hidden in the subfunctions it calls) for both cases a) and b).
    //
    // Note that all this applies ONLY to HumanPlayerT::Think()! All OTHER entities are NOT affected!
    // **********************************************************************************************************************************************

    for (unsigned int PCNr = 0; PCNr < m_PlayerCommands.Size(); PCNr++)
    {
        Think(m_PlayerCommands[PCNr], true /*ThinkingOnServerSide*/);
    }

    m_PlayerCommands.Overwrite();


    // It is important that we advance the time on the server-side GUI, too, so that it can
    // for example work off the "pending interpolations" that the GUI scripts can create.
    //
    // TODO: Check if this is true, especially in the light of client prediction.
    //       Maybe we should move all HUD GUI code into its own component, thereby
    //       isolating it from all other Human Player concerns, especially prediction?!
    //
    if (GetGuiHUD() != NULL)
        GetGuiHUD()->DistributeClockTickEvents(t);
}


void ComponentHumanPlayerT::DoClientFrame(float t)
{
    if (GetGuiHUD() != NULL)
        GetGuiHUD()->DistributeClockTickEvents(t);

    // TODO: Rendering the HUD GUI should probably be moved into some PostRender() method...
    if (GetGuiHUD() == NULL) return;

    MatSys::Renderer->PushMatrix(MatSys::RendererI::PROJECTION);
    MatSys::Renderer->PushMatrix(MatSys::RendererI::MODEL_TO_WORLD);
    MatSys::Renderer->PushMatrix(MatSys::RendererI::WORLD_TO_VIEW);

    const float zNear = 0.0f;
    const float zFar  = 1.0f;
    MatSys::Renderer->SetMatrix(MatSys::RendererI::PROJECTION,     MatrixT::GetProjOrthoMatrix(0.0f, cf::GuiSys::VIRTUAL_SCREEN_SIZE_X, cf::GuiSys::VIRTUAL_SCREEN_SIZE_Y, 0.0f, zNear, zFar));
    MatSys::Renderer->SetMatrix(MatSys::RendererI::MODEL_TO_WORLD, MatrixT());
    MatSys::Renderer->SetMatrix(MatSys::RendererI::WORLD_TO_VIEW,  MatrixT());

    GetGuiHUD()->Render();

    MatSys::Renderer->PopMatrix(MatSys::RendererI::PROJECTION);
    MatSys::Renderer->PopMatrix(MatSys::RendererI::MODEL_TO_WORLD);
    MatSys::Renderer->PopMatrix(MatSys::RendererI::WORLD_TO_VIEW);

    // // Handle any state driven effects of the currently carried weapon.
    // if (GetHaveWeapons() & (1 << GetActiveWeaponSlot()))
    // {
    //     GetCarriedWeapon(GetActiveWeaponSlot())->ClientSide_HandleStateDrivenEffects(this);
    // }
}


static const cf::TypeSys::MethsDocT META_GetActiveWeapon =
{
    "GetActiveWeapon",
    "Returns the ComponentCarriedWeaponT component of the currently active weapon,\n"
    "or `nil` if currently no weapon is active.",
    "", "()"
};

int ComponentHumanPlayerT::GetActiveWeapon(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<ComponentHumanPlayerT> Comp = Binder.GetCheckedObjectParam< IntrusivePtrT<ComponentHumanPlayerT> >(1);
    IntrusivePtrT<ComponentCarriedWeaponT> CarriedWeapon = Comp->GetActiveWeapon();

    if (CarriedWeapon == NULL)
    {
        lua_pushnil(LuaState);
        return 1;
    }

    Binder.Push(CarriedWeapon);
    return 1;
}


static const cf::TypeSys::MethsDocT META_SelectWeapon =
{
    "SelectWeapon",
    "This method initiates the holstering of the currently active weapon and the subsequent drawing\n"
    "of the given weapon.\n"
    "\n"
    "If the current weapon is unknown or not available to the player (e.g. because it has never been picked up),\n"
    "or if it turns out that the weapon does not support holstering (e.g. because there is no holstering\n"
    "sequence available), the holstering is skipped and the next weapon is drawn immediately.\n"
    "If the current weapon is fine but is not idle at the time that this method is called (e.g. reloading\n"
    "or firing), the call is *ignored*, that is, the weapon is *not* changed.\n"
    "\n"
    "@param NextWeapon   This can be the index number into the CarriedWeapon components of this entity, starting at 1.\n"
    "                    Use 0 to select \"no\" weapon.\n"
    "                    Alternatively, pass an instance of the carried weapon that is to be selected next.\n"
    "@param Force        If `true`, forces the drawing of the next weapon immediately, ignoring the idle\n"
    "                    state and holstering sequence of the current weapon. This is normally only used\n"
    "                    if, for example, the last hand grenade has been thrown and bare-handed animation\n"
    "                    sequences for idle and holster are not available.\n",
    "", "(any NextWeapon, bool Force)"
};

int ComponentHumanPlayerT::SelectWeapon(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<ComponentHumanPlayerT> Comp = Binder.GetCheckedObjectParam< IntrusivePtrT<ComponentHumanPlayerT> >(1);

    if (lua_isnumber(LuaState, 2))
    {
        Comp->SelectWeapon(lua_tointeger(LuaState, 2), lua_toboolean(LuaState, 3) != 0);
        return 0;
    }

    IntrusivePtrT<ComponentCarriedWeaponT> CarriedWeapon = Binder.GetCheckedObjectParam< IntrusivePtrT<ComponentCarriedWeaponT> >(2);
    const ArrayT< IntrusivePtrT<ComponentBaseT> >& Components = Comp->GetEntity()->GetComponents();
    uint8_t cwNr = 1;

    for (unsigned int i = 0; i < Components.Size(); i++)
    {
        IntrusivePtrT<const ComponentCarriedWeaponT> cw = dynamic_pointer_cast<const ComponentCarriedWeaponT>(Components[i]);

        if (cw == NULL) continue;
        if (cw == CarriedWeapon) break;

        cwNr++;
    }

    Comp->SelectWeapon(cwNr, lua_toboolean(LuaState, 3) != 0);
    return 0;
}


static const cf::TypeSys::MethsDocT META_SelectNextWeapon =
{
    "SelectNextWeapon",
    "This method draws the next weapon as previously prepared by SelectWeapon().\n"
    "\n"
    "It is intended to be called at the end of the holstering sequence of the previous weapon, either\n"
    "directly from SelectWeapon() when it found that holstering can entirely be skipped, or indirectly\n"
    "when SelectWeapon() calls the previous weapon's `Holster()` callback, the end of the holster\n"
    "sequence is detected in the `OnSequenceWrap_Sv()` script callback, and its implementation in turn\n"
    "calls this method.",
    "", "()"
};

int ComponentHumanPlayerT::SelectNextWeapon(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<ComponentHumanPlayerT> Comp = Binder.GetCheckedObjectParam< IntrusivePtrT<ComponentHumanPlayerT> >(1);

    Comp->SelectNextWeapon();
    return 0;
}


static const cf::TypeSys::MethsDocT META_FireRay =
{
    "FireRay",
    "Traces a ray through the world and causes damage to the hit entity (if any).\n\n"
    "This method can be used to implement the \"fire\" action of weapons that cause instantaneous damage,\n"
    "such as pistols, guns, rifles, etc.\n"
    "The ray is traced from the camera's origin along the camera's view vector, which can be randomly\n"
    "scattered (used to simulate inaccurate human aiming) by the given parameter `Random`.\n"
    "If an entity is hit, InflictDamage() is called with the hit entity and the amount of damage as given\n"
    "by parameter `Damage`.\n"
    "@param Damage   The damage to inflict to a possibly hit entity.\n"
    "@param Random   The maximum amount of random scatter to apply to the traced ray.",
    "", "(number Damage, number Random = 0.0)"
};

int ComponentHumanPlayerT::FireRay(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<ComponentHumanPlayerT> Comp = Binder.GetCheckedObjectParam< IntrusivePtrT<ComponentHumanPlayerT> >(1);

    const float  Damage = float(luaL_checknumber(LuaState, 2));
    const double Random = lua_tonumber(LuaState, 3);

    const Vector3dT  ViewDir = Comp->GetCameraViewDirWS(Random);
    const RayResultT RayResult(Comp->TraceCameraRay(ViewDir));

    if (RayResult.hasHit() && RayResult.GetHitPhysicsComp())
        Comp->InflictDamage(RayResult.GetHitPhysicsComp()->GetEntity(), Damage, ViewDir);

    return 0;
}


static const cf::TypeSys::MethsDocT META_GetRandom =
{
    "GetRandom",
    "Returns a pseudo-random number.\n\n"
    "If `n` is 0, 1, or absent (`nil`), this method returns a pseudo-random number in range `[0.0, 1.0]` (inclusive).\n"
    "Otherwise, a pseudo-random *integer* in range `0 ... n-1` is returned.\n\n"
    "The important aspect of this method is that it returns pseudo-random numbers that are reproducible in the\n"
    "context of the \"client prediction\" feature of the Cafu Engine. All random numbers that are used in human\n"
    "player code must be obtained from this method.",
    "", "(number n)"
};

int ComponentHumanPlayerT::GetRandom(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<ComponentHumanPlayerT> Comp = Binder.GetCheckedObjectParam< IntrusivePtrT<ComponentHumanPlayerT> >(1);
    const int n = luaL_optint(LuaState, 2, 0);

    if (n > 1)
    {
        lua_pushinteger(LuaState, Comp->GetRandom(n));
    }
    else
    {
        lua_pushnumber(LuaState, Comp->GetRandom());
    }

    return 1;
}


static const cf::TypeSys::MethsDocT META_SpawnWeaponChild =
{
    "SpawnWeaponChild",
    "An auxiliary method for spawning entities for thrown hand grenades, thrown face-huggers, launched AR grenades,\n"
    "or launched rockets (RPGs).\n\n"
    "This is only an auxiliary method -- it should in fact be removed and entirely be implemented in Lua instead!",
    "", "(string EntityName)"
};

int ComponentHumanPlayerT::SpawnWeaponChild(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<ComponentHumanPlayerT> HumanPlayer = Binder.GetCheckedObjectParam< IntrusivePtrT<ComponentHumanPlayerT> >(1);
    const std::string                    EntityName  = luaL_checkstring(LuaState, 2);

    IntrusivePtrT<cf::GameSys::EntityT> Ent = new cf::GameSys::EntityT(cf::GameSys::EntityCreateParamsT(HumanPlayer->GetEntity()->GetWorld()));
    HumanPlayer->GetEntity()->GetWorld().GetRootEntity()->AddChild(Ent);

    Ent->GetBasics()->SetEntityName(EntityName);

    // The challege is to find an initial NewEntOrigin so that the HumanPlayer's bounding-box (of the PlayerPhysics component)
    // and the new entity's bounding-box (of the PlayerPhysics component) don't intersect.
    // The code below tries this, but in a *very* flimsy and unreliable manner. It would be much better if, for example,
    // the new entity's PlayerPhysics components would be able to ignore not only their own collision model, but also that
    // of the HumanPlayer instance... TODO!
    const double    RadiusPlayerBB = 16.0;
    const double    RadiusNewEntBB =  4.0;      // Some use 3.0, some use 4.0, so let's use the maximum...
    const double    Safety         =  1.0;
    const Vector3dT ViewDir        = HumanPlayer->GetCameraViewDirWS();                             // TODO: Should ViewDir's "pitch" angle be limited to +/- 45° ?
    const Vector3dT ViewOffset     = ViewDir * (2.0 * (RadiusPlayerBB + RadiusNewEntBB) + Safety);  // == ViewDir * 41.0
    const Vector3dT NewEntOrigin   = HumanPlayer->GetCameraOriginWS() + ViewOffset;

    if (EntityName == "ARGrenade")
    {
        Ent->GetTransform()->SetOriginWS(NewEntOrigin.AsVectorOfFloat() - Vector3fT(0.0f, 0.0f, 10.0f));
        Ent->GetTransform()->SetQuatWS(HumanPlayer->GetEntity()->GetTransform()->GetQuatWS());

        IntrusivePtrT<cf::GameSys::ComponentPlayerPhysicsT> PlayerPhysicsComp = new cf::GameSys::ComponentPlayerPhysicsT();
        PlayerPhysicsComp->SetMember("Velocity", HumanPlayer->GetPlayerVelocity() + scale(ViewDir, 800.0));
        PlayerPhysicsComp->SetMember("Dimensions", BoundingBox3dT(Vector3dT(3.0, 3.0, 6.0), Vector3dT(-3.0, -3.0, 0.0)));
        Ent->AddComponent(PlayerPhysicsComp);
    }
    else if (EntityName == "FaceHugger")
    {
        Ent->GetTransform()->SetOriginWS(NewEntOrigin.AsVectorOfFloat());
        Ent->GetTransform()->SetQuatWS(HumanPlayer->GetEntity()->GetTransform()->GetQuatWS());

        IntrusivePtrT<cf::GameSys::ComponentPlayerPhysicsT> PlayerPhysicsComp = new cf::GameSys::ComponentPlayerPhysicsT();
        PlayerPhysicsComp->SetMember("Velocity", HumanPlayer->GetPlayerVelocity() + scale(ViewDir, 280.0));
        PlayerPhysicsComp->SetMember("Dimensions", BoundingBox3dT(Vector3dT( 4.0,  4.0, 4.0), Vector3dT(-4.0, -4.0, 0.0)));
        Ent->AddComponent(PlayerPhysicsComp);
    }
    else if (EntityName == "Grenade")
    {
        Ent->GetTransform()->SetOriginWS(NewEntOrigin.AsVectorOfFloat() + Vector3fT(0.0f, 0.0f, 10.0f));
        Ent->GetTransform()->SetQuatWS(HumanPlayer->GetEntity()->GetTransform()->GetQuatWS());

        IntrusivePtrT<cf::GameSys::ComponentPlayerPhysicsT> PlayerPhysicsComp = new cf::GameSys::ComponentPlayerPhysicsT();
        PlayerPhysicsComp->SetMember("Velocity", HumanPlayer->GetPlayerVelocity() + scale(ViewDir, 400.0));
        PlayerPhysicsComp->SetMember("Dimensions", BoundingBox3dT(Vector3dT(3.0, 3.0, 6.0), Vector3dT(-3.0, -3.0, 0.0)));
        Ent->AddComponent(PlayerPhysicsComp);
    }
    else if (EntityName == "Rocket")
    {
        Ent->GetTransform()->SetOriginWS(NewEntOrigin.AsVectorOfFloat() - Vector3fT(0.0f, 0.0f, 8.0f));
        Ent->GetTransform()->SetQuatWS(HumanPlayer->GetEntity()->GetChildren()[0]->GetTransform()->GetQuatWS());

        // This is not needed: Rocket physics are simple, implemented in the rocket's script code.
        // IntrusivePtrT<cf::GameSys::ComponentPlayerPhysicsT> PlayerPhysicsComp = new cf::GameSys::ComponentPlayerPhysicsT();
        // PlayerPhysicsComp->SetMember("Velocity", HumanPlayer->GetPlayerVelocity() + scale(ViewDir, 560.0));
        // PlayerPhysicsComp->SetMember("Dimensions", BoundingBox3dT(Vector3dT(4.0, 4.0, 4.0), Vector3dT(-4.0, -4.0, -4.0)));
        // Ent->AddComponent(PlayerPhysicsComp);
    }

    Binder.Push(Ent);
    return 1;
}


namespace
{
    static bool ParticleFunction_ShotgunHitWall(ParticleMST* Particle, float Time)
    {
        const float MaxAge = 0.4f;

        Particle->Age += Time;
        if (Particle->Age > MaxAge) return false;

        const float p = 1.0f - Particle->Age / MaxAge;     // % of remaining lifetime

        Particle->Color[0] = char( 20.0f * p);
        Particle->Color[1] = char(255.0f * p);
        Particle->Color[2] = char(180.0f * p);

        return true;
    }


    static bool ParticleFunction_HitWall(ParticleMST* Particle, float Time)
    {
        const float MaxAge = 3.0f;

        Particle->Age += Time;
        if (Particle->Age > MaxAge) return false;

        const float p = 1.0f - Particle->Age / MaxAge;     // % of remaining lifetime.

        Particle->Color[0] = char( 20.0f * p);
        Particle->Color[1] = char(180.0f * p);
        Particle->Color[2] = char(255.0f * p);

        return true;
    }


    static bool ParticleFunction_HitEntity(ParticleMST* Particle, float Time)
    {
        const float MaxAge = 1.0f;

        Particle->Age += Time;
        if (Particle->Age > MaxAge) return false;

        const float p = 1.0f - Particle->Age / MaxAge;     // % of remaining lifetime.

        Particle->Color[0] = char(255.0f * p);
        Particle->Color[1] = 0;
        Particle->Color[2] = 0;

        return true;
    }


    static bool ParticleFunction_ShotgunWhiteSmoke(ParticleMST* Particle, float Time)
    {
        const float FPS    = 20.0f;          // The default value is 20.0.
        const float MaxAge = 32.0f / FPS;    // 32 frames at 20 FPS.

        Particle->Origin[0] += Particle->Velocity[0] * Time;
        Particle->Origin[1] += Particle->Velocity[1] * Time;
        Particle->Origin[2] += Particle->Velocity[2] * Time;

        Particle->Radius += Time * 40.0f;

        const unsigned long MatNr = (unsigned long)(Particle->Age * FPS);
        assert(MatNr < Particle->AllRMs->Size());
        Particle->RenderMat = (*Particle->AllRMs)[MatNr];

        Particle->Age += Time;
        if (Particle->Age >= MaxAge) return false;

        return true;
    }
}


static const cf::TypeSys::MethsDocT META_RegisterParticle =
{
    "RegisterParticle",
    "An auxiliary method for spawning new particles.\n"
    "This is only a clumsy auxiliary method -- the entire particle system needs a thorough revision instead!",
    "", "(number Type)"
};

int ComponentHumanPlayerT::RegisterParticle(lua_State* LuaState)
{
    ScriptBinderT Binder(LuaState);
    IntrusivePtrT<ComponentHumanPlayerT> HumanPlayer = Binder.GetCheckedObjectParam< IntrusivePtrT<ComponentHumanPlayerT> >(1);
    const std::string Type = luaL_checkstring(LuaState, 2);

    if (Type == "shotgun-ray")
    {
        const Vector3dT  ViewDir = HumanPlayer->GetCameraViewDirWS(0.08748866);   // ca. 5°
        const RayResultT RayResult(HumanPlayer->TraceCameraRay(ViewDir));

        if (!RayResult.hasHit()) return 0;

        // Register a new particle at the 'Hit' point.
        ParticleMST NewParticle;

        NewParticle.Origin[0]=PhysToUnits(RayResult.m_hitPointWorld.x());
        NewParticle.Origin[1]=PhysToUnits(RayResult.m_hitPointWorld.y());
        NewParticle.Origin[2]=PhysToUnits(RayResult.m_hitPointWorld.z());

        NewParticle.Velocity[0]=0;
        NewParticle.Velocity[1]=0;
        NewParticle.Velocity[2]=0;

        NewParticle.Age=0.0;
        NewParticle.Color[3]=0;

        NewParticle.Radius=12.0;
        NewParticle.StretchY=1.0;
        NewParticle.AllRMs = NULL;
        NewParticle.RenderMat = HumanPlayer->m_GenericMatSet->GetRenderMats()[0];
        NewParticle.MoveFunction=RayResult.GetHitPhysicsComp()==NULL ? ParticleFunction_ShotgunHitWall : ParticleFunction_HitEntity;

        ParticleEngineMS::RegisterNewParticle(NewParticle);
    }
    else if (Type == "shotgun-smoke-1")
    {
        const Vector3dT ViewDir = HumanPlayer->GetCameraViewDirWS();

        // Register a new particle as "muzzle flash".
        ParticleMST NewParticle;

        NewParticle.Origin[0]=float(HumanPlayer->GetCameraOriginWS().x + ViewDir.x*16.0);
        NewParticle.Origin[1]=float(HumanPlayer->GetCameraOriginWS().y + ViewDir.y*16.0);
        NewParticle.Origin[2]=float(HumanPlayer->GetCameraOriginWS().z + ViewDir.z*16.0-4.0);

        NewParticle.Velocity[0]=float(ViewDir.x*40.0);
        NewParticle.Velocity[1]=float(ViewDir.y*40.0);
        NewParticle.Velocity[2]=float(ViewDir.z*40.0);

        NewParticle.Age=0.0;
        NewParticle.Color[0]=0;     // TODO: char(LastSeenAmbientColor.x*255.0);
        NewParticle.Color[1]=255;   // TODO: char(LastSeenAmbientColor.y*255.0);
        NewParticle.Color[2]=0;     // TODO: char(LastSeenAmbientColor.z*255.0);
        NewParticle.Color[3]=100;

        NewParticle.Radius=3.2f;
        NewParticle.Rotation=char(rand());
        NewParticle.StretchY=1.0;
        NewParticle.AllRMs = &HumanPlayer->m_WhiteSmokeMatSet->GetRenderMats();
        NewParticle.RenderMat = HumanPlayer->m_WhiteSmokeMatSet->GetRenderMats()[0];
        NewParticle.MoveFunction=ParticleFunction_ShotgunWhiteSmoke;

        ParticleEngineMS::RegisterNewParticle(NewParticle);
    }
    else if (Type == "shotgun-smoke-2")
    {
        const Vector3dT ViewDir = HumanPlayer->GetCameraViewDirWS();

        // Register a new particle as "muzzle flash".
        ParticleMST NewParticle;

        NewParticle.Origin[0]=float(HumanPlayer->GetCameraOriginWS().x + ViewDir.x*16.0);
        NewParticle.Origin[1]=float(HumanPlayer->GetCameraOriginWS().y + ViewDir.y*16.0);
        NewParticle.Origin[2]=float(HumanPlayer->GetCameraOriginWS().z + ViewDir.z*16.0-4.0);

        NewParticle.Velocity[0]=float(ViewDir.x*60.0);
        NewParticle.Velocity[1]=float(ViewDir.y*60.0);
        NewParticle.Velocity[2]=float(ViewDir.z*60.0);

        NewParticle.Age=0.0;
        NewParticle.Color[0]=0;     // TODO: char(LastSeenAmbientColor.x*255.0);
        NewParticle.Color[1]=255;   // TODO: char(LastSeenAmbientColor.y*255.0);
        NewParticle.Color[2]=0;     // TODO: char(LastSeenAmbientColor.z*255.0);
        NewParticle.Color[3]=180;

        NewParticle.Radius=8.0;
        NewParticle.Rotation=char(rand());
        NewParticle.StretchY=1.0;
        NewParticle.AllRMs = &HumanPlayer->m_WhiteSmokeMatSet->GetRenderMats();
        NewParticle.RenderMat = HumanPlayer->m_WhiteSmokeMatSet->GetRenderMats()[0];
        NewParticle.MoveFunction=ParticleFunction_ShotgunWhiteSmoke;

        ParticleEngineMS::RegisterNewParticle(NewParticle);
    }
    else if (Type == "AR-ray")
    {
        const Vector3dT  ViewDir = HumanPlayer->GetCameraViewDirWS(0.03492);  // ca. 2°
        const RayResultT RayResult(HumanPlayer->TraceCameraRay(ViewDir));

        if (!RayResult.hasHit()) return 0;

        // Register a new particle at the hit point.
        ParticleMST NewParticle;

        NewParticle.Origin[0]=PhysToUnits(RayResult.m_hitPointWorld.x());
        NewParticle.Origin[1]=PhysToUnits(RayResult.m_hitPointWorld.y());
        NewParticle.Origin[2]=PhysToUnits(RayResult.m_hitPointWorld.z());

        NewParticle.Velocity[0]=0;
        NewParticle.Velocity[1]=0;
        NewParticle.Velocity[2]=0;

        NewParticle.Age=0.0;
        NewParticle.Color[3]=0;

        NewParticle.Radius=12.0;
        NewParticle.StretchY=1.0;
        NewParticle.AllRMs = NULL;
        NewParticle.RenderMat = HumanPlayer->m_GenericMatSet->GetRenderMats()[0];
        NewParticle.MoveFunction=RayResult.GetHitPhysicsComp()==NULL ? ParticleFunction_HitWall : ParticleFunction_HitEntity;

        ParticleEngineMS::RegisterNewParticle(NewParticle);
    }
    else if (Type == "DesertEagle-ray")
    {
        const Vector3dT  ViewDir = HumanPlayer->GetCameraViewDirWS();
        const RayResultT RayResult(HumanPlayer->TraceCameraRay(ViewDir));

        if (!RayResult.hasHit()) return 0;

        // Register a new particle at the 'Hit' point.
        ParticleMST NewParticle;

        NewParticle.Origin[0]=PhysToUnits(RayResult.m_hitPointWorld.x());
        NewParticle.Origin[1]=PhysToUnits(RayResult.m_hitPointWorld.y());
        NewParticle.Origin[2]=PhysToUnits(RayResult.m_hitPointWorld.z());

        NewParticle.Velocity[0]=0;
        NewParticle.Velocity[1]=0;
        NewParticle.Velocity[2]=0;

        NewParticle.Age=0.0;
        NewParticle.Color[3]=0;

        NewParticle.Radius=12.0;
        NewParticle.StretchY=1.0;

        NewParticle.AllRMs = NULL;
        NewParticle.RenderMat = HumanPlayer->m_GenericMatSet->GetRenderMats()[0];
        NewParticle.MoveFunction=RayResult.GetHitPhysicsComp()==NULL ? ParticleFunction_HitWall : ParticleFunction_HitEntity;

        ParticleEngineMS::RegisterNewParticle(NewParticle);
    }

    return 0;
}


static const cf::TypeSys::MethsDocT META_toString =
{
    "__tostring",
    "This method returns a readable string representation of this object.",
    "string", "()"
};

int ComponentHumanPlayerT::toString(lua_State* LuaState)
{
    // ScriptBinderT Binder(LuaState);
    // IntrusivePtrT<ComponentBaseT> Comp = Binder.GetCheckedObjectParam< IntrusivePtrT<ComponentBaseT> >(1);

    lua_pushfstring(LuaState, "human player component");
    return 1;
}


/***********************************/
/*** TypeSys-related definitions ***/
/***********************************/

void* ComponentHumanPlayerT::CreateInstance(const cf::TypeSys::CreateParamsT& Params)
{
    return new ComponentHumanPlayerT();
}

const luaL_Reg ComponentHumanPlayerT::MethodsList[] =
{
    { "GetActiveWeapon",  GetActiveWeapon },
    { "SelectWeapon",     SelectWeapon },
    { "SelectNextWeapon", SelectNextWeapon },
    { "FireRay",          FireRay },
    { "GetRandom",        GetRandom },
    { "SpawnWeaponChild", SpawnWeaponChild },
    { "RegisterParticle", RegisterParticle },
    { "__tostring",       toString },
    { NULL, NULL }
};

const cf::TypeSys::MethsDocT ComponentHumanPlayerT::DocMethods[] =
{
    META_GetActiveWeapon,
    META_SelectWeapon,
    META_SelectNextWeapon,
    META_FireRay,
    META_GetRandom,
    META_SpawnWeaponChild,
    META_RegisterParticle,
    META_toString,
    { NULL, NULL, NULL, NULL }
};

const cf::TypeSys::TypeInfoT ComponentHumanPlayerT::TypeInfo(GetComponentTIM(), "GameSys::ComponentHumanPlayerT", "GameSys::ComponentBaseT", ComponentHumanPlayerT::CreateInstance, MethodsList, DocClass, DocMethods, DocVars);

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

#include "cw_RPG.hpp"
#include "HumanPlayer.hpp"
#include "Constants_AmmoSlots.hpp"
#include "Constants_WeaponSlots.hpp"
#include "GameSys/CompLightPoint.hpp"
#include "GameSys/CompModel.hpp"
#include "GameSys/CompParticleSystemOld.hpp"
#include "GameSys/CompScript.hpp"
#include "GameSys/CompSound.hpp"
#include "Math3D/Angles.hpp"
#include "Libs/LookupTables.hpp"
#include "../../GameWorld.hpp"
#include "Models/ModelManager.hpp"

using namespace GAME_NAME;


CarriedWeaponRPGT::CarriedWeaponRPGT(ModelManagerT& ModelMan)
    : CarriedWeaponT(ModelMan.GetModel("Games/DeathMatch/Models/Weapons/Bazooka/Bazooka_v.cmdl"),
                     ModelMan.GetModel("Games/DeathMatch/Models/Weapons/Bazooka/Bazooka_p.cmdl"))
{
}


bool CarriedWeaponRPGT::ServerSide_PickedUpByEntity(EntHumanPlayerT* Player) const
{
    EntityStateT& State=Player->GetState();

    // Consider if the entity already has this weapon.
    if (State.HaveWeapons & (1 << WEAPON_SLOT_RPG))
    {
        // If it also has the max. amount of ammo of this type, ignore the touch.
        if (State.HaveAmmo[AMMO_SLOT_ROCKETS]==5) return false;

        // Otherwise pick the weapon up and let it have the ammo.
        State.HaveAmmo[AMMO_SLOT_ROCKETS]+=1;
    }
    else
    {
        // This weapon is picked up for the first time.
        State.HaveWeapons|=1 << WEAPON_SLOT_RPG;
        State.ActiveWeaponSlot   =WEAPON_SLOT_RPG;
        State.ActiveWeaponSequNr =5;    // Draw
        State.ActiveWeaponFrameNr=0.0;

        State.HaveAmmoInWeapons[WEAPON_SLOT_RPG  ] =1;
        State.HaveAmmo         [AMMO_SLOT_ROCKETS]+=0;
    }

    // Limit the amount of carryable ammo.
    if (State.HaveAmmo[AMMO_SLOT_ROCKETS]>5) State.HaveAmmo[AMMO_SLOT_ROCKETS]=5;

    return true;
}


void CarriedWeaponRPGT::ServerSide_Think(EntHumanPlayerT* Player, const PlayerCommandT& PlayerCommand, bool ThinkingOnServerSide, unsigned long ServerFrameNr, bool AnimSequenceWrap) const
{
    EntityStateT& State=Player->GetState();

    switch (State.ActiveWeaponSequNr)
    {
        case 0: // Idle1 (rocket inserted)
        case 1: // Fidget1 (rocket inserted)
            if (PlayerCommand.Keys & (PCK_Fire1 | PCK_Fire2))
            {
                State.ActiveWeaponSequNr =3;    // Fire
                State.ActiveWeaponFrameNr=0.0;

                // Important: ONLY create (throw) a new rocket IF we are on the server side!
                if (ThinkingOnServerSide)
                {
                    // Clamp 'Pitch' values larger than 45° (==8192) to 45°.
                    const unsigned short Pitch=(Player->GetPitch()>8192 && Player->GetPitch()<=16384) ? 8192 : Player->GetPitch();

                    const float ViewDirZ=-LookupTables::Angle16ToSin[Pitch];
                    const float ViewDirY= LookupTables::Angle16ToCos[Pitch];

                    // Note: There is a non-trivial relationship between heading, pitch, and the corresponding view vector.
                    // Especially does a heading and pitch of 45° NOT correspond to the view vector (1, 1, 1), and vice versa!
                    // Think carefully about this before changing the number 41.0 below (which actually is 2.0*(16.0+4.0) (+1.0 for "safety")).
                    const VectorT ViewDir(ViewDirY*LookupTables::Angle16ToSin[Player->GetHeading()], ViewDirY*LookupTables::Angle16ToCos[Player->GetHeading()], ViewDirZ);
                    const VectorT RocketOrigin(Player->GetOrigin()-VectorT(0.0, 0.0, 8.0)+scale(ViewDir, 41.0)+scale(State.Velocity, double(PlayerCommand.FrameTime)));
                    std::map<std::string, std::string> Props;

                    Props["classname"]="monster_rocket";

                    unsigned long RocketID=Player->GameWorld->CreateNewEntity(Props, ServerFrameNr, RocketOrigin);

                    if (RocketID!=0xFFFFFFFF)
                    {
                        IntrusivePtrT<BaseEntityT> Rocket = dynamic_pointer_cast<BaseEntityT>(Player->GameWorld->GetGameEntityByID(RocketID));
                        IntrusivePtrT<cf::GameSys::EntityT> Ent = Rocket->m_Entity;

                        Ent->GetBasics()->SetEntityName("Rocket");
                        Ent->GetTransform()->SetOrigin(RocketOrigin.AsVectorOfFloat());
                        Ent->GetTransform()->SetQuat(cf::math::QuaternionfT::Euler(
                            float((Player->GetPitch()/8192.0*45.0) * 3.1415926 / 180.0),
                            float((90.0 - Player->GetHeading()/8192.0*45.0) * 3.1415926 / 180.0),
                            0));

                        IntrusivePtrT<cf::GameSys::ComponentModelT> ModelComp = new cf::GameSys::ComponentModelT();
                        ModelComp->SetMember("Name", std::string("Games/DeathMatch/Models/Weapons/Grenade/Grenade_w.cmdl"));
                        Ent->AddComponent(ModelComp);

#if 0
                        // This is not needed: Rocket physics are simple, implemented in our script code.
                        IntrusivePtrT<cf::GameSys::ComponentPlayerPhysicsT> PlayerPhysicsComp = new cf::GameSys::ComponentPlayerPhysicsT();
                        PlayerPhysicsComp->SetMember("Velocity", State.Velocity + scale(ViewDir, 560.0));
                        PlayerPhysicsComp->SetMember("Dimensions", BoundingBox3dT(Vector3dT(4.0, 4.0, 4.0), Vector3dT(-4.0, -4.0, -4.0)));
                        Ent->AddComponent(PlayerPhysicsComp);
#endif

                        IntrusivePtrT<cf::GameSys::ComponentParticleSystemOldT> PaSysComp1 = new cf::GameSys::ComponentParticleSystemOldT();
                        PaSysComp1->SetMember("Type", std::string("Rocket_Expl_main"));
                        Ent->AddComponent(PaSysComp1);

                        IntrusivePtrT<cf::GameSys::ComponentParticleSystemOldT> PaSysComp2 = new cf::GameSys::ComponentParticleSystemOldT();
                        PaSysComp2->SetMember("Type", std::string("Rocket_Expl_sparkle"));
                        Ent->AddComponent(PaSysComp2);

                        IntrusivePtrT<cf::GameSys::ComponentPointLightT> LightComp = new cf::GameSys::ComponentPointLightT();
                        LightComp->SetMember("On", true);
                        LightComp->SetMember("Color", Vector3fT(1.0f, 0.9f, 0.0f));
                        LightComp->SetMember("Radius", 500.0f);
                        // Shadows are activated only at the time of the explosion (when the model is hidden),
                        // because at this time, our light source origin is at the center of the model, which does not
                        // agree well with shadow casting. Proper solution can be:
                        //   - exempt the model from casting shadows,
                        //   - offset the light source from the model center, e.g. by `-ViewDir * 16.0`.
                        // The latter is what we did in pre-component-system versions of the code, but now it would
                        // require the employment of a child entity.
                        LightComp->SetMember("ShadowType", int(cf::GameSys::ComponentPointLightT::VarShadowTypeT::NONE));
                        Ent->AddComponent(LightComp);

                        IntrusivePtrT<cf::GameSys::ComponentSoundT> SoundComp = new cf::GameSys::ComponentSoundT();
                        SoundComp->SetMember("Name", std::string("Weapon/Shotgun_dBarrel"));
                        SoundComp->SetMember("AutoPlay", false);
                        Ent->AddComponent(SoundComp);

                        IntrusivePtrT<cf::GameSys::ComponentScriptT> ScriptComp = new cf::GameSys::ComponentScriptT();
                        ScriptComp->SetMember("Name", std::string("Games/DeathMatch/Scripts/Rocket.lua"));
                        Ent->AddComponent(ScriptComp);

                        // As we're inserting a new entity into a live map, post-load stuff must be run here.
                        ScriptComp->OnPostLoad(false);
                    }
                }
                break;
            }

            if (AnimSequenceWrap)
            {
                if (State.ActiveWeaponSequNr==0)
                {
                    State.ActiveWeaponSequNr=LookupTables::RandomUShort[PlayerCommand.Nr & 0xFFF] & 1;
                }
                else State.ActiveWeaponSequNr=0;     // Don't play the "Fidget" sequence repeatedly.

                State.ActiveWeaponFrameNr=0.0;
            }
            break;

        case 2: // Reload
            if (AnimSequenceWrap)
            {
                State.ActiveWeaponSequNr =0;
                State.ActiveWeaponFrameNr=0.0;
            }
            break;

        case 3: // Fire
            if (AnimSequenceWrap)
            {
                State.ActiveWeaponSequNr =2;
                State.ActiveWeaponFrameNr=0.0;
            }
            break;

        case 4: // Holster1 (rocket inserted)
            break;

        case 5: // Draw1 (rocket inserted)
            if (AnimSequenceWrap)
            {
                State.ActiveWeaponSequNr =0;
                State.ActiveWeaponFrameNr=0.0;
            }
            break;

        case 6: // Holster2 (empty)
            break;

        case 7: // Draw2 (empty)
            if (AnimSequenceWrap)
            {
                State.ActiveWeaponSequNr =8;
                State.ActiveWeaponFrameNr=0.0;
            }
            break;

        case 8: // Idle2 (empty)
        case 9: // Fidget2 (empty)
            break;
    }
}

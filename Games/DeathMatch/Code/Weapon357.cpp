/*
=================================================================================
This file is part of Cafu, the open-source game engine and graphics engine
for multiplayer, cross-platform, real-time 3D action.
Copyright (C) 2002-2012 Carsten Fuchs Software.

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

/*************************/
/*** Weapon 357 (Code) ***/
/*************************/

#include "Weapon357.hpp"
#include "cw.hpp"
#include "Constants_AmmoSlots.hpp"
#include "Constants_WeaponSlots.hpp"
#include "EntityCreateParams.hpp"
#include "GameImpl.hpp"
#include "HumanPlayer.hpp"
#include "TypeSys.hpp"


// Implement the type info related code.
const cf::TypeSys::TypeInfoT* EntWeapon357T::GetType() const
{
    return &TypeInfo;
 // return &EntWeapon357T::TypeInfo;
}

void* EntWeapon357T::CreateInstance(const cf::TypeSys::CreateParamsT& Params)
{
    return new EntWeapon357T(*static_cast<const EntityCreateParamsT*>(&Params));
}

const cf::TypeSys::TypeInfoT EntWeapon357T::TypeInfo(GetBaseEntTIM(), "EntWeapon357T", "EntWeaponT", EntWeapon357T::CreateInstance, NULL /*MethodsList*/);


EntWeapon357T::EntWeapon357T(const EntityCreateParamsT& Params)
    : EntWeaponT(Params, "Games/DeathMatch/Models/Weapons/DesertEagle/DesertEagle_w.cmdl")
{
}


void EntWeapon357T::NotifyTouchedBy(BaseEntityT* Entity)
{
    // If we are touched by anything else than a human player, ignore the touch.
    // Would be interesting to also allow touchs by bots, though.
    if (Entity->GetType()!=&EntHumanPlayerT::TypeInfo) return;

    // If we are touched when not being "active", ignore the touch.
    if (!IsActive()) return;

    // Give this weapon to the entity.
    if (!cf::GameSys::GameImplT::GetInstance().GetCarriedWeapon(WEAPON_SLOT_357)->ServerSide_PickedUpByEntity(dynamic_cast<EntHumanPlayerT*>(Entity))) return;

    // And finally retire for a while.
    PostEvent(EVENT_TYPE_PICKED_UP);
    Deactivate(5.0f);
}

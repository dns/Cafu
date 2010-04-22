/*
=================================================================================
This file is part of Cafu, the open-source game and graphics engine for
multiplayer, cross-platform, real-time 3D action.
$Id$

Copyright (C) 2002-2010 Carsten Fuchs Software.

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

/**********************/
/*** Carried Weapon ***/
/**********************/

#ifndef _CARRIEDWEAPON_HPP_
#define _CARRIEDWEAPON_HPP_

#include "Math3D/Vector3.hpp"


class  BaseEntityT;
class  EntHumanPlayerT;
struct PlayerCommandT;
class  ModelProxyT;


// The ONLY purpose of this interface is to localize (group) the code and data
// that is related to a picked-up weapon (carried by an entity).
// It is intended to simplify the other code (mostly of the human player entity).
// Don't confuse this with the weapon entities that represent weapons-to-be-picked-up,
// this code is NOT related to a specific entity!
class CarriedWeaponT
{
    public:

    // This function returns a pointer to the CarriedWeaponT class for the desired ActiveWeaponSlot.
    // When no such class exists, it returns a pointer to an empty dummy implementation
    // (but for the convenience of the caller, it never returns NULL or an invalid pointer).
    static const CarriedWeaponT* GetCarriedWeapon(char ActiveWeaponSlot);



    // This function indicates the ammo that this weapon consumes for primary fire.
    // The returned value is an index into the State.HaveAmmo array (of the human player entity).
    // THIS FUNCTION IS PROBABLY OF NO USE, BECAUSE ITS INFORMATION IS PROCESSED INTERNALLY.
    // virtual char GetAmmoSlotForPrimaryFire() const;

    // This function indicates the ammo that this weapon consumes for secondary fire.
    // The returned value is an index into the State.HaveAmmo array (of the human player entity).
    // THIS FUNCTION IS PROBABLY OF NO USE, BECAUSE ITS INFORMATION IS PROCESSED INTERNALLY.
    // virtual char GetAmmoSlotForSecondaryFire() const;

    // This functions returns the model of the VIEW weapon model (1st persons view) of this weapon.
    virtual ModelProxyT& GetViewWeaponModel() const=0;

    // This functions returns the model of the PLAYER weapon model (3rd persons view) of this weapon (as a submodel to the players body model).
    virtual ModelProxyT& GetPlayerWeaponModel() const=0;



    // This function is to be called when the entity 'Entity' has just picked up this weapon.
    // Returns true if the weapon was successfully given to (picked up by) the entity, false otherwise.
    // Usually called by a weapon entity (that represents the weapon lying around) on detection of touch by a human player.
    virtual bool ServerSide_PickedUpByEntity(BaseEntityT* Entity) const;

    // This functions handles the thinking for this carried weapon.
    // Typically called from within EntHumanPlayerT::Think().
    virtual void ServerSide_Think(EntHumanPlayerT* Player, const PlayerCommandT& PlayerCommand, bool ThinkingOnServerSide, unsigned long ServerFrameNr, bool AnimSequenceWrap) const;



    // This function handles the occurance of a "primary fire" event that was received by Entity.
    virtual void ClientSide_HandlePrimaryFireEvent(const EntHumanPlayerT* Player, const VectorT& LastSeenAmbientColor) const;

    // This function handles the occurance of a "secondary fire" event that was received by Entity.
    virtual void ClientSide_HandleSecondaryFireEvent(const EntHumanPlayerT* Player, const VectorT& LastSeenAmbientColor) const;

    // Function for handling state (vs. event) driven effects of the weapon of Player,
    // e.g. drawing laser beams, keeping the sound on the weapon sound channel alive, and so on.
    virtual void ClientSide_HandleStateDrivenEffects(const EntHumanPlayerT* Player) const;

 // virtual void ClientSide_DrawCrossHair(BaseEntityT* Entity, int Seed,int OpenGLWindow_Width, int OpenGLWindow_Height); //for hud effect
 // virtual void ClientSide_GetAmmoString(BaseEntityT* Entity, int Seed);



    // The destructor.
    virtual ~CarriedWeaponT() { }
};

#endif
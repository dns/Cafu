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

#ifndef CAFU_MAP_ENTITY_HPP_INCLUDED
#define CAFU_MAP_ENTITY_HPP_INCLUDED

#include "MapEntityBase.hpp"


class MapEntityT : public MapEntityBaseT
{
    public:

    /// The default constructor.
    MapEntityT(MapDocumentT& MapDoc);

    /// The copy constructor for copying an entity.
    /// @param Entity   The entity to copy-construct this entity from.
    MapEntityT(const MapEntityT& Entity);


    // Implementations and overrides for base class methods.
    MapEntityT* Clone() const;

    BoundingBox3fT GetBB() const;


    // The TypeSys related declarations for this class.
    virtual const cf::TypeSys::TypeInfoT* GetType() const { return &TypeInfo; }
    static void* CreateInstance(const cf::TypeSys::CreateParamsT& Params);
    static const cf::TypeSys::TypeInfoT TypeInfo;
};

#endif

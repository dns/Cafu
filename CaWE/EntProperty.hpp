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

#ifndef CAFU_ENT_PROPERTY_HPP_INCLUDED
#define CAFU_ENT_PROPERTY_HPP_INCLUDED

#include "Math3D/Vector3.hpp"
#include "wx/string.h"
#include <ostream>


class TextParserT;


class EntPropertyT
{
    public:

    EntPropertyT(const wxString& Key_="", const wxString& Value_="") : Key(Key_), Value(Value_) { }

    void Load_cmap(TextParserT& TP);
    void Save_cmap(std::ostream& OutFile) const;

    Vector3fT GetVector3f() const;


    wxString Key;
    wxString Value;
 // const EntClassVarT* Var;    //CF: Should we have such a member here? Saves us searching all variables of the entity class...
};

#endif

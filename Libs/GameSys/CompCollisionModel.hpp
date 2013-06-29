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

#ifndef CAFU_GAMESYS_COMPONENT_COLLISION_MODEL_HPP_INCLUDED
#define CAFU_GAMESYS_COMPONENT_COLLISION_MODEL_HPP_INCLUDED

#include "CompBase.hpp"


namespace cf
{
    namespace GameSys
    {
        /// This component adds a collision model to its entity.
        class ComponentCollisionModelT : public ComponentBaseT
        {
            public:

            /// The constructor.
            ComponentCollisionModelT();

            /// The copy constructor.
            /// @param Comp   The component to create a copy of.
            ComponentCollisionModelT(const ComponentCollisionModelT& Comp);

            /// Sets the file name of the collision model.
            void SetCollMdlName(const std::string& FileName) { m_CollMdlName.Set(FileName); }

            // Base class overrides.
            ComponentCollisionModelT* Clone() const;
            const char* GetName() const { return "CollisionModel"; }


            // The TypeSys related declarations for this class.
            const cf::TypeSys::TypeInfoT* GetType() const { return &TypeInfo; }
            static void* CreateInstance(const cf::TypeSys::CreateParamsT& Params);
            static const cf::TypeSys::TypeInfoT TypeInfo;


            protected:

            // The Lua API methods of this class.
            static int toString(lua_State* LuaState);

            static const luaL_Reg               MethodsList[];  ///< The list of Lua methods for this class.
            static const char*                  DocClass;
            static const cf::TypeSys::MethsDocT DocMethods[];
            static const cf::TypeSys::VarsDocT  DocVars[];


            private:

            TypeSys::VarT<std::string> m_CollMdlName;   ///< The file name of the collision model.
        };
    }
}

#endif
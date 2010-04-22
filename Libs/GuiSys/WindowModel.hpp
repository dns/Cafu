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

#ifndef _CF_GUISYS_WINDOW_MODEL_HPP_
#define _CF_GUISYS_WINDOW_MODEL_HPP_

#include "Window.hpp"
#include "Models/Model_proxy.hpp"


namespace cf
{
    namespace GuiSys
    {
        /// A window that renders a model.
        class ModelWindowT : public WindowT
        {
            public:

            /// Constructor for creating a model window.
            ModelWindowT(const cf::GuiSys::WindowCreateParamsT& Params);

            /// The Copy Constructor.
            ModelWindowT(const ModelWindowT& Window, bool Recursive=false);

            virtual ModelWindowT* Clone(bool Recursive=false) const;

            /// Destructor.
            ~ModelWindowT();

            /// Returns the model this window shows.
            ModelProxyT& GetModel() { return Model; }


            // Overloaded methods from the base class.
            void Render() const;
            bool OnClockTickEvent(float t);
            void EditorFillInPG(wxPropertyGridManager* PropMan);
            bool UpdateProperty(wxPGProperty* Property);
            bool EditorHandlePGChange(wxPropertyGridEvent& Event, GuiEditor::ChildFrameT* ChildFrame);
            bool WriteInitMethod(std::ostream& OutFile);
            void EditorRender() const;


            // The TypeSys related declarations for this class.
            virtual const cf::TypeSys::TypeInfoT* GetType() const { return &TypeInfo; }
            static void* CreateInstance(const cf::TypeSys::CreateParamsT& Params);
            static const cf::TypeSys::TypeInfoT TypeInfo;


            protected:

            void FillMemberVars(); ///< Helper method that fills the MemberVars array with entries for each class member.


            private:

            ModelProxyT Model;          ///< The model instance rendered by this window.
            int         ModelSequNr;    ///< The animation sequence number that is used for the model.
            float       ModelFrameNr;   ///< The frame number of the sequence. The only variable that is not set by script, but advanced automatically.
            Vector3fT   ModelPos;       ///< The position of the model in world space.
            float       ModelScale;     ///< The scale factor applied to the model coordinates when converted to world space.
            Vector3fT   ModelAngles;    ///< The angles around the axes that determine the orientation of the model in world space.
            Vector3fT   CameraPos;      ///< The position of the camera in world space.


            // Lua script methods.
            static int SetModel(lua_State* LuaState);
            static int GetModelNrOfSequs(lua_State* LuaState);
            static int SetModelSequNr(lua_State* LuaState);
            static int SetModelPos(lua_State* LuaState);
            static int SetModelScale(lua_State* LuaState);
            static int SetModelAngles(lua_State* LuaState);
            static int SetCameraPos(lua_State* LuaState);

            static const luaL_Reg MethodsList[];
        };
    }
}

#endif